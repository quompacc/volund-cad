#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <Bnd_Box.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M11 (A-BODY-02, user decision 2026-10-15): a box 20 x 10 x 10 and an extrusion of the same
// rectangle, each moved by (30, -10, 5) and turned 90 degrees about Z, stand alike. The box
// keeps its own placement; the extrusion gets one move ("Bewegen 1"), changed by the second
// drag. Undo takes back one drag. A sketch on the moved top face and a cut from it make a hole
// where the face is now. A box with a fillet after it gets a move as well. Save/open.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ExtrusionMode;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
std::array<double, 6> bounds(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    std::array<double, 6> result{};
    box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
    return result;
}
bool same(const std::array<double, 6>& a, const std::array<double, 6>& b) {
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::abs(a[i] - b[i]) > 1e-6)
            return false;
    return true;
}
const TopoDS_Shape& bodyOf(const application::CadSession& session, document::BodyId body) {
    for (const auto& item : session.validBodies())
        if (item.bodyId == body)
            return item.shape;
    throw std::runtime_error("Body missing");
}
bool solidAt(const TopoDS_Shape& shape, double x, double y, double z) {
    return BRepClass3d_SolidClassifier(shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN;
}
std::size_t moves(const application::CadSession& session) {
    return static_cast<std::size_t>(std::ranges::count(
        session.features(), document::FeatureKind::move, &document::FeatureDescriptor::kind));
}
application::CommandResult move(application::CadSession& session, document::FeatureId feature,
                                double x, double y, double z) {
    return application::executeAction(
               session,
               application::prepareRequest(
                   session, application::MoveBodyAction{feature,
                                                        {x, application::QuantityUnit::millimetre},
                                                        {y, application::QuantityUnit::millimetre},
                                                        {z, application::QuantityUnit::millimetre},
                                                        std::nullopt,
                                                        {0.0, application::QuantityUnit::radian}}))
        .command;
}
application::CommandResult turnAboutZ(application::CadSession& session, document::FeatureId feature,
                                      double angle) {
    return session.moveBody(feature, {0, 0, 0}, 2, angle);
}

void moveBodies() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 10, 10}).feature();
    document::SketchParameters rectangle{document::datumSketchPlane(document::DatumPlane::xy), {}};
    rectangle.datum = document::DatumPlane::xy;
    rectangle.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}});
    rectangle.elements.nextId = 2;
    const auto regions = features::identifySketchRegions(rectangle.elements.values).ids;
    require(session.createDatumSketchExtrusion(rectangle, regions, 10).status() ==
                CommandStatus::committed,
            "Extrusion of the same rectangle");
    const auto extrusion = session.features().back().id;
    const auto boxBody = *session.resultBody(box);
    const auto extrusionBody = *session.resultBody(extrusion);
    require(same(bounds(bodyOf(session, boxBody)), bounds(bodyOf(session, extrusionBody))),
            "Both stand alike at the start");

    require(move(session, box, 30, -10, 5).status() == CommandStatus::committed &&
                turnAboutZ(session, box, pi / 2).status() == CommandStatus::committed &&
                moves(session) == 0,
            "The box moves by its own placement");
    require(move(session, extrusion, 30, -10, 5).status() == CommandStatus::committed &&
                moves(session) == 1 && session.features().back().name == "Bewegen 1",
            "The extrusion gets a move");
    const auto moved = session.features().back().id;
    require(turnAboutZ(session, extrusion, pi / 2).status() == CommandStatus::committed &&
                moves(session) == 1,
            "The second drag changes that move");
    const std::array<double, 6> expected{35, -15, 5, 45, 5, 15};
    require(same(bounds(bodyOf(session, boxBody)), expected) &&
                same(bounds(bodyOf(session, extrusionBody)), expected),
            "Box and extrusion stand alike, moved and turned");
    require(session.undo().status() == CommandStatus::committed &&
                same(bounds(bodyOf(session, extrusionBody)), {30, -10, 5, 50, 0, 15}),
            "Undo takes back the turn of the extrusion only");
    require(session.redo().status() == CommandStatus::committed, "Redo");

    // A sketch on the moved top face and a cut from it: the hole is where the face is now.
    const auto faces = session.supportFaces(moved);
    const auto top = std::ranges::find_if(faces, [](const auto& face) {
        return near(face.plane.normalZ, 1.0) && near(face.plane.originZ, 15.0);
    });
    require(top != faces.end(), "The moved top face");
    auto sketch = session.sketchOnFace(moved, top->origin);
    const double cx = (top->minX + top->maxX) / 2, cy = (top->minY + top->maxY) / 2;
    sketch.elements = {{{document::SketchElementId{1}, document::SketchCircle{{cx, cy}, 2}}}, 2};
    const auto sketched = session.createSketchFeature(moved, sketch);
    require(sketched.status() == CommandStatus::committed, "Sketch on the moved face");
    const auto sketchId = session.features().back().id;
    const auto holes =
        features::identifySketchRegions(session.sketchParameters(sketchId).elements.values).ids;
    require(session.createSketchProfileExtrusion(
                       sketchId, holes, 5, ExtrusionMode{document::ExtrusionOperation::cut, true})
                    .status() == CommandStatus::committed,
            "Cut from the moved face");
    const auto& cut = bodyOf(session, extrusionBody);
    require(near(test::volumeOf(cut), 2000 - 20 * pi) && !solidAt(cut, 40, -5, 13) &&
                solidAt(cut, 40, -5, 8) && solidAt(cut, 37, -5, 13) &&
                BRepCheck_Analyzer(cut).IsValid(),
            "The hole sits in the middle of the moved top face");

    // Once a fillet follows on the box, a drag adds a move rather than changing the box.
    const document::FilletEdge edge{
        {box, kernel::PrimitiveRole::boxPositiveZ}, {box, kernel::PrimitiveRole::boxPositiveX}, 1};
    require(session.createFillet(box, std::span{&edge, 1}, 1).status() == CommandStatus::committed,
            "A fillet on the box");
    const auto before = bounds(bodyOf(session, boxBody));
    require(move(session, box, 0, 0, 10).status() == CommandStatus::committed &&
                moves(session) == 2 && session.features().back().name == "Bewegen 2" &&
                near(bounds(bodyOf(session, boxBody))[2], before[2] + 10),
            "The filleted box gets a move");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m11-move";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "move.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(
        reopened.moveParameters(moved) == session.moveParameters(moved) &&
            same(bounds(bodyOf(reopened, extrusionBody)), bounds(bodyOf(session, extrusionBody))) &&
            same(bounds(bodyOf(reopened, boxBody)), bounds(bodyOf(session, boxBody))),
        "Open restores the moves");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        moveBodies();
        std::cout << "PASS M11 move: box and extrusion moved and turned alike, one move per body, "
                     "undo, cut on the moved face, move after a fillet, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
