#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

// M13 (A-PART-01, A-HISTORY-01): the reference plate 60 x 40 x 10 from an XY rectangle with a
// through hole of 8 at (12, 0), a pocket (-24, -10)..(-10, 10) 4 deep and one rounded vertical
// edge at (-30, -20); a box stands apart. Widening the early rectangle to 70 carries the whole
// chain: hole and pocket keep their places, the fillet sits at the new corner, the box stays.
// A preview of the change leaves the document as it was; undo restores the whole chain. A width
// that leaves hole and pocket off the plate is refused, or kept with errors on request; no later
// feature is deleted, and undo restores the plate.
// M14 (A-HISTORY-02, user decision 2026-10-16): suppressing the pocket passes the plate on to
// the fillet; a chamfer on a pocket edge then fails rather than guessing another edge; the
// state survives save/open with all IDs and parameters, and reactivating restores the plate.
// Suppressing the plate's extrusion removes the body and blocks its features; the box stays.
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

const TopoDS_Shape& bodyOf(const application::CadSession& session, document::BodyId body) {
    for (const auto& item : session.validBodies())
        if (item.bodyId == body)
            return item.shape;
    throw std::runtime_error("Body missing");
}
bool solidAt(const TopoDS_Shape& shape, double x, double y, double z) {
    return BRepClass3d_SolidClassifier(shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN;
}

// The top face of `support` and a point of the model in that face's frame.
struct Top final {
    document::SubshapeOrigin origin;
    document::SketchPlane plane;
    [[nodiscard]] std::pair<double, double> local(double x, double y) const {
        const double dx = x - plane.originX, dy = y - plane.originY;
        const double yX = plane.normalY * plane.xDirectionZ - plane.normalZ * plane.xDirectionY;
        const double yY = plane.normalZ * plane.xDirectionX - plane.normalX * plane.xDirectionZ;
        return {dx * plane.xDirectionX + dy * plane.xDirectionY, dx * yX + dy * yY};
    }
};
Top top(const application::CadSession& session, document::FeatureId support) {
    for (const auto& face : session.supportFaces(support))
        if (face.plane.normalZ > 0.5 && near(face.plane.originZ, 10))
            return {face.origin, face.plane};
    throw std::runtime_error("Top face missing");
}

document::SketchParameters plateSketch(double halfWidth) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{-halfWidth, -20, halfWidth, 20}});
    sketch.elements.nextId = 2;
    return sketch;
}

struct Plate final {
    document::FeatureId sketch;
    document::BodyId body;
    document::BodyId box;
    document::FeatureId extrusion{1};
    document::FeatureId hole{1};
    document::FeatureId pocket{1};
    document::FeatureId fillet{1};
};

Plate referencePlate(application::CadSession& session) {
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    auto apart = session.identifiedSnapshot().snapshot().boxParameters(box);
    apart.placement.translationX = 100;
    require(session.editBoxParameters(box, apart).status() == CommandStatus::committed,
            "A box apart");
    const auto rectangle = plateSketch(30);
    require(session.createDatumSketchExtrusion(
                    rectangle, features::identifySketchRegions(rectangle.elements.values).ids, 10)
                    .status() == CommandStatus::committed,
            "Plate 60 x 40 x 10");
    const auto extrusion = session.features().back().id;
    Plate plate{session.features()[session.features().size() - 2].id,
                *session.resultBody(extrusion), *session.resultBody(box)};
    plate.extrusion = extrusion;
    require(near(test::volumeOf(bodyOf(session, plate.body)), 24000), "24000");

    const auto face = top(session, extrusion);
    const auto [hx, hy] = face.local(12, 0);
    require(session.createHole(extrusion, face.origin, hx, hy, 8, std::nullopt).status() ==
                CommandStatus::committed,
            "Hole of 8 at (12, 0)");
    const auto hole = session.features().back().id;
    require(near(test::volumeOf(bodyOf(session, plate.body)), 24000 - 160 * pi), "24000 - 160 pi");

    const auto pocketFace = top(session, hole);
    auto pocket = session.sketchOnFace(hole, pocketFace.origin);
    const auto toPocket = [&](double x, double y) {
        const double dx = x - pocket.plane.originX, dy = y - pocket.plane.originY;
        const double yX =
            pocket.plane.normalY * pocket.plane.xDirectionZ - pocket.plane.normalZ * pocket.plane.xDirectionY;
        const double yY =
            pocket.plane.normalZ * pocket.plane.xDirectionX - pocket.plane.normalX * pocket.plane.xDirectionZ;
        return std::pair{dx * pocket.plane.xDirectionX + dy * pocket.plane.xDirectionY,
                         dx * yX + dy * yY};
    };
    const auto [x0, y0] = toPocket(-24, -10);
    const auto [x1, y1] = toPocket(-10, 10);
    pocket.elements = {{{document::SketchElementId{1},
                         document::SketchRectangle{std::min(x0, x1), std::min(y0, y1),
                                                   std::max(x0, x1), std::max(y0, y1)}}},
                       2};
    require(session.createSketchFeature(hole, pocket).status() == CommandStatus::committed,
            "Pocket sketch");
    const auto pocketSketch = session.features().back().id;
    require(session
                    .createSketchProfileExtrusion(
                        pocketSketch,
                        features::identifySketchRegions(
                            session.sketchParameters(pocketSketch).elements.values)
                            .ids,
                        4, ExtrusionMode{document::ExtrusionOperation::cut, true})
                    .status() == CommandStatus::committed,
            "Pocket 4 deep");
    const auto cut = session.features().back().id;
    plate.hole = hole;
    plate.pocket = cut;
    require(near(test::volumeOf(bodyOf(session, plate.body)), 22880 - 160 * pi),
            "22880 - 160 pi");

    // The vertical edge at (-30, -20), named by its two faces.
    std::optional<document::FilletEdge> corner;
    for (TopExp_Explorer explorer(bodyOf(session, plate.body), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (near(middle.X(), -30) && near(middle.Y(), -20) && near(middle.Z(), 5))
            corner = session.filletEdge(cut, explorer.Current());
    }
    require(corner.has_value(), "The corner edge has a name");
    require(session.createFillet(cut, std::span{&*corner, 1}, 1).status() ==
                CommandStatus::committed,
            "Fillet 1 on the corner");
    require(near(test::volumeOf(bodyOf(session, plate.body)),
                 22880 - 160 * pi - 10 * (1 - pi / 4)),
            "22880 - 160 pi - 10 (1 - pi / 4)");
    plate.fillet = session.features().back().id;
    return plate;
}

double finished(double width) { return width * 400 - 1120 - 160 * pi - 10 * (1 - pi / 4); }

void earlyChange() {
    application::CadSession session;
    const auto plate = referencePlate(session);
    const auto features = session.features().size();
    const auto revision = session.sceneStatus().document;

    // A preview of the change leaves the document as it was.
    const auto preview = application::previewAction(
        session, application::prepareRequest(
                     session, application::EditSketchAction{plate.sketch, plateSketch(35)}));
    require(preview.result.command.status() == CommandStatus::committed &&
                session.sceneStatus().document == revision &&
                near(test::volumeOf(bodyOf(session, plate.body)), finished(60)),
            "The preview changes nothing");

    require(session.editSketchParameters(plate.sketch, plateSketch(35)).status() ==
                CommandStatus::committed,
            "Width 60 -> 70");
    const auto& wide = bodyOf(session, plate.body);
    require(session.features().size() == features && near(test::volumeOf(wide), finished(70)) &&
                BRepCheck_Analyzer(wide).IsValid(),
            "The whole chain follows the wider plate");
    require(!solidAt(wide, 12, 0, 5) && solidAt(wide, 12, 5, 5) && !solidAt(wide, -17, 0, 8) &&
                solidAt(wide, -17, 0, 3) && !solidAt(wide, -34.9, -19.9, 5) &&
                solidAt(wide, 34.9, -19.9, 5),
            "Hole and pocket keep their places; the fillet sits at the new corner");
    require(near(test::volumeOf(bodyOf(session, plate.box)), 1000), "The box stays");

    require(session.undo().status() == CommandStatus::committed &&
                near(test::volumeOf(bodyOf(session, plate.body)), finished(60)) &&
                !solidAt(bodyOf(session, plate.body), -29.9, -19.9, 5),
            "Undo restores the whole chain");

    // Too narrow: hole and pocket would miss the plate.
    const auto refused = session.editSketchParameters(plate.sketch, plateSketch(8));
    require(refused.status() == CommandStatus::rejected && session.features().size() == features &&
                near(test::volumeOf(bodyOf(session, plate.body)), finished(60)),
            "A width that loses hole and pocket is refused");
    const auto kept = session.editSketchParameters(plate.sketch, plateSketch(8),
                                                   application::ErrorDisposition::keepEditableErrors);
    std::size_t failed = 0;
    for (const auto& state : session.featureStates())
        failed += state.status != application::FeatureRuntimeStatus::current ? 1 : 0;
    require(kept.status() == CommandStatus::committedWithErrors &&
                session.features().size() == features && failed >= 2,
            "Kept on request with errors; no later feature is deleted");
    require(session.undo().status() == CommandStatus::committed &&
                near(test::volumeOf(bodyOf(session, plate.body)), finished(60)),
            "Undo restores the valid plate");
}

application::FeatureRuntimeStatus stateOf(const application::CadSession& session,
                                          document::FeatureId feature) {
    for (const auto& state : session.featureStates())
        if (state.id == feature)
            return state.status;
    throw std::runtime_error("Feature state missing");
}

void suppression() {
    using application::FeatureRuntimeStatus;
    application::CadSession session;
    const auto plate = referencePlate(session);
    const double withoutPocket = finished(60) + 1120;
    require(session.setFeatureSuppressed(plate.pocket, true).status() == CommandStatus::committed &&
                stateOf(session, plate.pocket) == FeatureRuntimeStatus::suppressed &&
                stateOf(session, plate.fillet) == FeatureRuntimeStatus::current &&
                near(test::volumeOf(bodyOf(session, plate.body)), withoutPocket),
            "Without the pocket the fillet still rounds the corner");
    require(session.undo().status() == CommandStatus::committed &&
                stateOf(session, plate.pocket) == FeatureRuntimeStatus::current &&
                near(test::volumeOf(bodyOf(session, plate.body)), finished(60)),
            "Suppressing is one undo step");

    // A chamfer on a rim edge of the pocket.
    std::optional<document::FilletEdge> rim;
    for (TopExp_Explorer explorer(bodyOf(session, plate.body), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (near(middle.X(), -17) && near(middle.Y(), -10) && near(middle.Z(), 10))
            rim = session.filletEdge(plate.fillet, explorer.Current());
    }
    require(rim && session.createChamfer(plate.fillet, std::span{&*rim, 1}, 0.5).status() ==
                       CommandStatus::committed,
            "A chamfer on the pocket rim");
    const auto chamfer = session.features().back().id;
    const auto all = session.features(); // one copy: begin and end of the same range
    const std::vector<document::FeatureDescriptor> features(all.begin(), all.end());
    require(session.setFeatureSuppressed(plate.pocket, true).status() ==
                    CommandStatus::committedWithErrors &&
                stateOf(session, chamfer) == FeatureRuntimeStatus::error &&
                stateOf(session, plate.fillet) == FeatureRuntimeStatus::current,
            "Without the pocket its rim is gone: the chamfer fails, no other edge is guessed");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m14-suppress";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "plate.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    const auto restored = reopened.features();
    require(std::ranges::equal(restored, features,
                               [](const auto& a, const auto& b) {
                                   return a.id == b.id && a.kind == b.kind;
                               }) &&
                stateOf(reopened, plate.pocket) == FeatureRuntimeStatus::suppressed &&
                stateOf(reopened, chamfer) == FeatureRuntimeStatus::error,
            "Open keeps the suppressed pocket, all IDs and the chamfer's error");
    require(reopened.setFeatureSuppressed(plate.pocket, false).status() ==
                    CommandStatus::committed &&
                stateOf(reopened, chamfer) == FeatureRuntimeStatus::current &&
                reopened.extrusionParameters(plate.pocket) == session.extrusionParameters(plate.pocket),
            "Reactivated: pocket and chamfer are back with their parameters");
    std::filesystem::remove_all(directory);

    require(reopened.setFeatureSuppressed(plate.extrusion, true).status() ==
                    CommandStatus::committedWithErrors &&
                reopened.validBodies().size() == 1 &&
                near(test::volumeOf(bodyOf(reopened, plate.box)), 1000) &&
                stateOf(reopened, plate.hole) == FeatureRuntimeStatus::blocked,
            "Without the plate's extrusion the body is gone, its features blocked; the box stays");
}

} // namespace

int main() {
    try {
        earlyChange();
        suppression();
        std::cout << "PASS M13 history (A-HISTORY-01): reference plate, width 60 -> 70 through "
                     "hole, pocket and fillet, box unchanged, preview without change, undo, "
                     "too narrow refused or kept with errors; suppress (A-HISTORY-02): pass-through, no "
                     "guessed edge, save/open, reactivate, body creator\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
