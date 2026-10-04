#include <voelund/application/Actions.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// AI02b, A-ACTION-01 "Auswahlwechsel": a request for body A changes only A, even when the
// GUI has selected B in the meantime; the selection itself stays B.
namespace {
using namespace voelund;
using application::ActionRejection;
using application::ActionRequest;
using application::ActionResult;
using application::CommandStatus;
using application::Quantity;
using application::QuantityUnit;
using document::FeatureId;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
Quantity mm(double value) { return {value, QuantityUnit::millimetre}; }
Quantity rad(double value) { return {value, QuantityUnit::radian}; }

ActionRequest request(const application::CadSession& session, application::Action action) {
    static int sequence = 0;
    return {"target-" + std::to_string(++sequence), session.documentId(),
            session.sceneStatus().document, std::move(action)};
}
ActionResult committed(application::CadSession& session, application::Action action,
                       const std::string& message) {
    auto result = application::executeAction(session, request(session, std::move(action)));
    require(result.command.status() == CommandStatus::committed &&
                result.rejection == ActionRejection::none,
            message + ": " + result.command.diagnostic());
    return result;
}
FeatureId created(application::CadSession& session, application::Action action,
                  const std::string& message) {
    const auto result = committed(session, std::move(action), message);
    require(result.createdFeatures.size() == 1, message + ": one new feature");
    return result.createdFeatures.front();
}
void changesOnly(const ActionResult& result, FeatureId feature, const std::string& message) {
    require(result.createdFeatures.empty() && result.changedFeatures == std::vector{feature},
            message + ": changes exactly the target");
}
void rejected(application::CadSession& session, application::Action action,
              ActionRejection expected, const std::string& message) {
    const auto before = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus().document;
    const auto result = application::executeAction(session, request(session, std::move(action)));
    require(result.command.status() == CommandStatus::rejected && result.rejection == expected &&
                session.identifiedSnapshot() == before && session.sceneStatus().document == stamp,
            message);
}

void selectionDoesNotDecide() {
    application::CadSession session;
    session.newDocument();
    const auto a = created(session, application::CreateBoxAction{mm(10), mm(20), mm(30)}, "Box A");
    const auto b = created(session, application::CreateBoxAction{mm(5), mm(5), mm(5)}, "Box B");
    require(session.selectFeature(b), "Select B");
    const auto document = [&] { return session.identifiedSnapshot().snapshot(); };
    const auto selectedB = [&] { return session.selectedFeature() == b; };

    changesOnly(committed(session,
                          application::SetBoxDimensionsAction{a, mm(12), mm(20), mm(30)},
                          "Resize A"),
                a, "Resize A");
    require(document().boxDimensions(a) == kernel::BoxDimensions{12, 20, 30} &&
                document().boxDimensions(b) == kernel::BoxDimensions{5, 5, 5} && selectedB(),
            "Only A is resized; B stays selected");

    changesOnly(committed(session,
                          application::SetPlacementAction{a, mm(100), mm(0), mm(0), rad(0),
                                                          rad(0), rad(0.5)},
                          "Move A"),
                a, "Move A");
    require(document().boxParameters(a).placement.translationX == 100 &&
                document().boxParameters(a).placement.rotationZ.value() == 0.5 &&
                document().boxParameters(b).placement.translationX == 0 && selectedB(),
            "Only A moves; B stays selected");

    // B is both the last feature and selected; the cylinder still goes onto A.
    const auto cylinder = created(
        session, application::CreateCylinderAction{a, document::CylinderResultMode::join},
        "Cylinder on A");
    const auto features = session.features();
    require(std::ranges::find(features, cylinder, &document::FeatureDescriptor::id)
                    ->inputFeature == a,
            "The cylinder uses A as input");
    require(session.selectFeature(b), "Select B again");
    changesOnly(committed(session, application::SetCylinderSizeAction{cylinder, mm(3), mm(7)},
                          "Resize cylinder"),
                cylinder, "Resize cylinder");
    require(document().cylinderParameters(cylinder).radius == 3 &&
                document().cylinderParameters(cylinder).height == 7 && selectedB(),
            "The cylinder is resized; B stays selected");
    // A joined cylinder is placed by its input body.
    changesOnly(committed(session,
                          application::SetPlacementAction{cylinder, mm(50), mm(0), mm(0), rad(0),
                                                          rad(0), rad(0)},
                          "Move A through the cylinder"),
                a, "Move A through the cylinder");
    require(document().boxParameters(a).placement.translationX == 50 && selectedB(),
            "The placement owner A moves");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    rejected(session, application::SetBoxDimensionsAction{cylinder, mm(1), mm(1), mm(1)},
             ActionRejection::unsupportedTarget, "Box dimensions on a cylinder");
    rejected(session, application::SetCylinderSizeAction{a, mm(1), mm(1)},
             ActionRejection::unsupportedTarget, "Cylinder size on a box");
    rejected(session, application::CreateCylinderAction{FeatureId{999},
                                                       document::CylinderResultMode::join},
             ActionRejection::unknownTarget, "Cylinder on a missing input");
    rejected(session,
             application::SetPlacementAction{a, mm(0), mm(0), mm(0), rad(0), rad(0),
                                             {30, QuantityUnit::degree}},
             ActionRejection::invalidUnit, "Placement rotation in degrees");
    rejected(session, application::CreateBoxAction{mm(1), mm(nan), mm(1)},
             ActionRejection::nonFinite, "Box with NaN depth");
    require(selectedB(), "Rejections keep the selection");
}
} // namespace

int main() {
    try {
        selectionDoesNotDecide();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
