#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

// AI02a: typed requests with explicit targets, the read-only action context and the checks
// before any transaction. Every rejection leaves stamp, document and undo history unchanged.
namespace {
using namespace voelund;
using application::ActionKind;
using application::ActionRejection;
using application::ActionRequest;
using application::ActionResult;
using application::CommandStatus;
using application::Quantity;
using application::QuantityUnit;
using document::FeatureId;
using document::ParameterId;
using document::ParameterUnit;
using document::SketchConstraintId;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
Quantity mm(double value) { return {value, QuantityUnit::millimetre}; }

ActionRequest request(const application::CadSession& session, application::Action action) {
    static int sequence = 0;
    return {"request-" + std::to_string(++sequence), session.documentId(),
            session.sceneStatus().document, std::move(action)};
}
ActionResult committed(application::CadSession& session, application::Action action,
                       const std::string& message) {
    auto result = application::executeAction(session, request(session, std::move(action)));
    require(result.command.status() == CommandStatus::committed &&
                result.rejection == ActionRejection::none &&
                result.stamp == session.sceneStatus().document,
            message + ": " + result.command.diagnostic());
    return result;
}

// A circle with its radius dimension as constraint 1.
document::SketchParameters circle(double radius) {
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchCircle{{0, 0}, radius}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    sketch.constraints.values.emplace_back(
        SketchConstraintId{1}, document::SketchConstraintKind::radius,
        document::SketchGeometryReference{document::SketchElementId{1},
                                          document::SketchPointPosition::center},
        std::nullopt, radius);
    sketch.constraints.nextId = 2;
    return sketch;
}

struct Model final {
    ParameterId length;
    ParameterId angle;
    FeatureId sketch;
    FeatureId extrusion;
};

Model build(application::CadSession& session) {
    auto result = committed(session,
                            application::CreateParameterAction{"A", ParameterUnit::length, mm(10), {}},
                            "Create A");
    require(result.createdParameters.size() == 1 && result.changedParameters.empty() &&
                result.createdFeatures.empty(),
            "Creating A reports exactly one new parameter");
    const auto length = result.createdParameters.front();
    result = committed(session,
                       application::CreateParameterAction{
                           "W", ParameterUnit::angle, {30, QuantityUnit::degree}, {}},
                       "Create W");
    const auto angle = result.createdParameters.front();

    result = committed(session, application::CreateDatumSketchAction{circle(10)}, "Sketch");
    require(result.createdFeatures.size() == 1 && result.command.feature() &&
                result.createdFeatures.front() == *result.command.feature(),
            "Sketch creation reports its feature");
    const auto sketch = result.createdFeatures.front();

    const auto context = application::describeActionContext(session);
    require(context.document == session.documentId() &&
                context.stamp == session.sceneStatus().document &&
                context.units.length == QuantityUnit::millimetre &&
                context.units.sketchAngle == QuantityUnit::radian &&
                context.units.angleParameter == QuantityUnit::degree &&
                context.parameters.values.size() == 2,
            "Context describes document, stamp, units and parameters");
    const auto regions = std::ranges::find(context.sketchRegions, sketch,
                                           &application::SketchRegionsSummary::sketch);
    require(regions != context.sketchRegions.end() && regions->valid &&
                regions->regions.size() == 1 && !regions->regions.front().ambiguous &&
                near(regions->regions.front().area, std::numbers::pi * 100),
            "Context lists the circle region with its area");
    const auto sketchActions =
        std::ranges::find_if(context.available, [&](const application::ActionAvailability& a) {
            const auto* target = std::get_if<FeatureId>(&a.target);
            return target && *target == sketch;
        });
    require(sketchActions != context.available.end() &&
                std::ranges::find(sketchActions->actions, ActionKind::createProfileExtrusion) !=
                    sketchActions->actions.end(),
            "Context offers a profile extrusion on the sketch");

    result = committed(session,
                       application::CreateProfileExtrusionAction{
                           sketch, {regions->regions.front().id}, mm(10)},
                       "Extrude");
    const auto extrusion = result.createdFeatures.front();
    result = committed(session, application::SetExtrusionDistanceAction{extrusion, mm(15)},
                       "Distance 15");
    require(result.changedFeatures == std::vector{extrusion} && result.createdFeatures.empty() &&
                near(test::volumeOf(session.currentShape()), std::numbers::pi * 100 * 15),
            "Changing the distance reports and changes only the extrusion");
    committed(session, application::BindExtrusionDistanceAction{extrusion, length}, "Bind");
    result = committed(session, application::SetParameterValueAction{length, mm(20)}, "A -> 20");
    require(result.changedParameters == std::vector{length} &&
                result.changedFeatures == std::vector{extrusion} &&
                near(test::volumeOf(session.currentShape()), std::numbers::pi * 100 * 20),
            "A drives the bound distance");
    return {length, angle, sketch, extrusion};
}

void rejections(application::CadSession& session, const Model& model) {
    const auto stamp = session.sceneStatus().document;
    const auto snapshot = session.identifiedSnapshot();
    const bool canUndo = session.canUndo();
    const bool canRedo = session.canRedo();
    const auto expect = [&](ActionRequest candidate, ActionRejection rejection,
                            const std::string& name) {
        const auto result = application::executeAction(session, candidate);
        require(result.command.status() == CommandStatus::rejected &&
                    result.rejection == rejection && result.stamp == stamp &&
                    result.createdFeatures.empty() && result.changedFeatures.empty() &&
                    result.createdParameters.empty() && result.changedParameters.empty(),
                name + ": rejection " + std::to_string(static_cast<int>(result.rejection)));
        require(session.sceneStatus().document == stamp && session.identifiedSnapshot() == snapshot &&
                    session.canUndo() == canUndo && session.canRedo() == canRedo,
                name + ": state unchanged");
    };
    const auto make = [&](application::Action action) { return request(session, std::move(action)); };

    auto foreign = make(application::SetParameterValueAction{model.length, mm(30)});
    foreign.document = document::generateDocumentId();
    expect(foreign, ActionRejection::foreignDocument, "Foreign document");
    foreign.expected.revision -= 1;
    expect(foreign, ActionRejection::foreignDocument, "Namespace before stamp");
    auto stale = make(application::SetParameterValueAction{model.length, mm(30)});
    stale.expected.revision -= 1;
    expect(stale, ActionRejection::staleStamp, "Stale revision");
    stale = make(application::SetParameterValueAction{model.length, mm(30)});
    stale.expected.generation += 1;
    expect(stale, ActionRejection::staleStamp, "Other generation");
    stale.expected = stamp;
    stale.action = application::SetParameterValueAction{ParameterId{99}, mm(nan)};
    expect(stale, ActionRejection::unknownTarget, "Existence before finiteness");

    expect(make(application::SetExtrusionDistanceAction{FeatureId{999}, mm(5)}),
           ActionRejection::unknownTarget, "Unknown feature");
    expect(make(application::RenameParameterAction{ParameterId{99}, "B"}),
           ActionRejection::unknownTarget, "Unknown parameter");
    expect(make(application::BindSketchDimensionAction{model.sketch, SketchConstraintId{42},
                                                       model.length}),
           ActionRejection::unknownTarget, "Unknown sketch dimension");
    expect(make(application::BindExtrusionDistanceAction{model.extrusion, ParameterId{99}}),
           ActionRejection::unknownTarget, "Unknown bound parameter");
    expect(make(application::SetExtrusionDistanceAction{model.sketch, {nan, QuantityUnit::degree}}),
           ActionRejection::unsupportedTarget, "Capability before unit and finiteness");
    expect(make(application::SetExtrusionDistanceAction{model.extrusion, mm(5)}),
           ActionRejection::unsupportedTarget, "A bound distance follows its parameter");
    expect(make(application::EditSketchAction{model.extrusion, circle(5)}),
           ActionRejection::unsupportedTarget, "Edit sketch on an extrusion");

    expect(make(application::SetParameterValueAction{model.length, {20, QuantityUnit::degree}}),
           ActionRejection::invalidUnit, "Length parameter in degrees");
    expect(make(application::SetParameterValueAction{model.angle, {0.5, QuantityUnit::radian}}),
           ActionRejection::invalidUnit, "Angle parameter in radians");
    expect(make(application::BindSketchDimensionAction{model.sketch, SketchConstraintId{1},
                                                       model.angle}),
           ActionRejection::invalidUnit, "Radius bound to an angle");
    expect(make(application::BindExtrusionDistanceAction{model.extrusion, model.angle}),
           ActionRejection::invalidUnit, "Distance bound to an angle");
    expect(make(application::CreateParameterAction{"B", ParameterUnit::length,
                                                   {inf, QuantityUnit::radian}, {}}),
           ActionRejection::invalidUnit, "Unit before finiteness");

    for (const double value : {nan, inf, -inf})
        expect(make(application::SetParameterValueAction{model.length, mm(value)}),
               ActionRejection::nonFinite, "Non-finite " + std::to_string(value));
    const auto missing = document::SketchRegionId::fromBoundaries(
        {{document::SketchElementId{77}, 0, std::nullopt}}, {});
    expect(make(application::CreateProfileExtrusionAction{model.sketch, {missing}, mm(nan)}),
           ActionRejection::nonFinite, "Finiteness before reference resolution");
    expect(make(application::CreateProfileExtrusionAction{model.sketch, {missing}, mm(5)}),
           ActionRejection::unresolvedReference, "Unresolved region");

    // Accepted by the action layer, refused by the command: still no mutation.
    expect(make(application::RenameParameterAction{model.angle, "a"}), ActionRejection::none,
           "Duplicate name is a command rejection");
    expect(make(application::SetParameterValueAction{model.length, mm(20)}),
           ActionRejection::none, "Unchanged value");

    application::CommandResult undone = session.undo();
    require(undone.status() == CommandStatus::committed &&
                document::findParameter(session.parameterTable(), model.length)->value == 10,
            "The undo step after all rejections is still A -> 20");
}

void staleAfterUndo(application::CadSession& session, const Model& model) {
    const auto prepared = request(session, application::SetParameterValueAction{model.length, mm(40)});
    require(session.redo().status() == CommandStatus::committed, "Redo A -> 20");
    const auto result = application::executeAction(session, prepared);
    require(result.rejection == ActionRejection::staleStamp &&
                document::findParameter(session.parameterTable(), model.length)->value == 20,
            "Redo makes a prepared request stale");
}
} // namespace

int main() {
    try {
        application::CadSession session;
        session.newDocument();
        const auto model = build(session);
        rejections(session, model);
        staleAfterUndo(session, model);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
