#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// M05 (A-FILLET-01, user decisions 2026-10-12): a fillet rounds edges of one body with one
// radius; each edge is stored as its two adjacent faces. One vertical 10 mm edge of a
// 20 x 20 x 10 box with radius 1 removes 10 (1 - pi/4) mm^3 and stays a valid solid. A radius
// too large is refused with the edge at fault and leaves the body unchanged. After the box
// grows the same edge is still rounded; Save/Open keeps the fillet. A vertical side edge of
// a sketch extrusion is rounded the same way.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;
const double removed = 10 * (1 - pi / 4);

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}
bool valid(const application::CadSession& session) {
    for (const auto& body : session.validBodies())
        if (!BRepCheck_Analyzer(body.shape).IsValid())
            return false;
    return !session.validBodies().empty();
}

// The face of `support` whose outward normal points along the axis with this sign.
document::SubshapeOrigin face(const application::CadSession& session, document::FeatureId support,
                              double nx, double ny, double nz) {
    for (const auto& candidate : session.supportFaces(support))
        if (std::abs(candidate.plane.normalX - nx) < 1e-9 &&
            std::abs(candidate.plane.normalY - ny) < 1e-9 &&
            std::abs(candidate.plane.normalZ - nz) < 1e-9)
            return candidate.origin;
    throw std::runtime_error("Face with this normal missing");
}
// The vertical edge where the +X and +Y faces meet.
document::FilletEdge corner(const application::CadSession& session, document::FeatureId support) {
    return {face(session, support, 1, 0, 0), face(session, support, 0, 1, 0)};
}

void boxEdge() {
    application::CadSession session;
    session.newDocument();
    const auto created = session.createBoxFeature({20, 20, 10});
    require(created.status() == CommandStatus::committed, "Box");
    const auto box = *created.feature();
    const auto edge = corner(session, box);

    const auto tooLarge = session.createFillet(box, std::span(&edge, 1), 25);
    require(tooLarge.status() == CommandStatus::rejected && session.features().size() == 1 &&
                near(volume(session), 4000),
            "A radius the edge cannot take is refused; the box stays");
    bool explained = false;
    for (const auto& issue : tooLarge.evaluation().features)
        explained = explained || (issue.code == "fillet-failed" && issue.elements ==
                                                                       std::vector<std::uint64_t>{1});
    require(explained, "The refusal names fillet-failed and edge 1");

    const auto rounded = session.createFillet(box, std::span(&edge, 1), 1);
    require(rounded.status() == CommandStatus::committed && rounded.feature().has_value(),
            "Radius 1 rounds the edge");
    const auto fillet = *rounded.feature();
    require(session.features().back().name == "Verrundung 1" &&
                near(volume(session), 4000 - removed) && valid(session),
            "Volume 4000 - 10 (1 - pi/4), valid solid");

    // One undo step.
    require(session.undo().status() == CommandStatus::committed && near(volume(session), 4000) &&
                session.redo().status() == CommandStatus::committed &&
                near(volume(session), 4000 - removed),
            "Undo and redo the fillet");

    // The box grows: the same named edge is rounded on the larger box.
    auto bigger = session.identifiedSnapshot().snapshot().boxParameters(box);
    bigger.dimensions.width = 30;
    require(session.editBoxParameters(box, bigger).status() == CommandStatus::committed &&
                near(volume(session), 6000 - removed) && valid(session),
            "After the box grows the edge is still rounded");

    // A larger radius on the existing fillet; too large is refused and keeps radius 1.
    auto parameters = session.filletParameters(fillet);
    parameters.radius = 2;
    require(session.editFilletParameters(fillet, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 6000 - 10 * (4 - pi)),
            "Radius 2 removes 10 (4 - pi)");
    parameters.radius = 25;
    require(session.editFilletParameters(fillet, parameters).status() ==
                    CommandStatus::rejected &&
                near(volume(session), 6000 - 10 * (4 - pi)),
            "Radius 25 is refused, radius 2 stays");

    // Save and Open keep the fillet (schema 14).
    const auto directory = std::filesystem::temp_directory_path() / "voelund-m05-fillet";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "fillet.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.features().size() == 2 &&
                reopened.features().back().kind == document::FeatureKind::fillet &&
                reopened.filletParameters(fillet) == session.filletParameters(fillet) &&
                near(volume(reopened), 6000 - 10 * (4 - pi)),
            "Open restores the fillet with its edge and radius");
    std::filesystem::remove_all(directory);
}

// M07 (user decisions 2026-10-12): a symmetric chamfer "Fase N" on the same named edge. A
// 1 mm chamfer of a 10 mm edge removes 10 / 2 = 5 mm^3, 2 mm removes 20; too large is refused
// with chamfer-failed and edge 1; the box grows and the edge stays; Save/Open (schema 15).
void chamferEdge() {
    application::CadSession session;
    session.newDocument();
    const auto created = session.createBoxFeature({20, 20, 10});
    require(created.status() == CommandStatus::committed, "Box");
    const auto box = *created.feature();
    const auto edge = corner(session, box);

    const auto tooLarge = session.createChamfer(box, std::span(&edge, 1), 25);
    bool explained = false;
    for (const auto& issue : tooLarge.evaluation().features)
        explained = explained || (issue.code == "chamfer-failed" &&
                                  issue.elements == std::vector<std::uint64_t>{1});
    require(tooLarge.status() == CommandStatus::rejected && explained &&
                session.features().size() == 1 && near(volume(session), 4000),
            "A distance the edge cannot take is refused with chamfer-failed and edge 1");

    const auto result = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateChamferAction{
                                  box, {edge}, {1, application::QuantityUnit::millimetre}}));
    require(result.command.status() == CommandStatus::committed &&
                session.features().back().name == "Fase 1" &&
                session.features().back().kind == document::FeatureKind::chamfer &&
                near(volume(session), 4000 - 5) && valid(session),
            "Distance 1 removes 5 mm^3, valid solid");
    const auto chamfer = session.features().back().id;
    require(session.undo().status() == CommandStatus::committed && near(volume(session), 4000) &&
                session.redo().status() == CommandStatus::committed &&
                near(volume(session), 4000 - 5),
            "Undo and redo the chamfer");

    auto bigger = session.identifiedSnapshot().snapshot().boxParameters(box);
    bigger.dimensions.width = 30;
    require(session.editBoxParameters(box, bigger).status() == CommandStatus::committed &&
                near(volume(session), 6000 - 5) && valid(session),
            "After the box grows the edge is still chamfered");
    auto parameters = session.chamferParameters(chamfer);
    parameters.distance = 2;
    require(session.editChamferParameters(chamfer, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 6000 - 20),
            "Distance 2 removes 20 mm^3");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m07-chamfer";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "chamfer.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.chamferParameters(chamfer) == session.chamferParameters(chamfer) &&
                near(volume(reopened), 6000 - 20),
            "Open restores the chamfer");
    std::filesystem::remove_all(directory);
}

void extrusionSideEdge() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{-40, -20, 40, 20}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    const auto plate = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateSketchExtrusionAction{
                                  sketch, {}, {10, application::QuantityUnit::millimetre}}));
    require(plate.createdFeatures.size() == 2, "Plate 80 x 40 x 10");
    const auto extrusion = plate.createdFeatures.back();
    const auto edge = corner(session, extrusion);
    // Through the action, as the shell and the assistant use it.
    const auto result = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateFilletAction{
                                  extrusion, {edge}, {1, application::QuantityUnit::millimetre}}));
    require(result.command.status() == CommandStatus::committed &&
                near(volume(session), 32000 - removed) && valid(session),
            "A vertical side edge of the extrusion is rounded");
}

} // namespace

int main() {
    try {
        boxEdge();
        chamferEdge();
        extrusionSideEdge();
        std::cout << "PASS M05/M07 fillet and chamfer: box edge 10 (1 - pi/4) and 5, refused, base change, "
                     "edit, save/open, extrusion side edge\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
