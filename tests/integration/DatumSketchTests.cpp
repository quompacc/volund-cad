#include "CadSessionTestSupport.hpp"
#include <voelund/application/SketchEditorSession.hpp>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// V01a: sketches on the base planes XY/XZ/YZ need no support body, open a new body and
// extrude without a fuse (A-EXT-01: 20 x 10 on XY, distance 8 -> 1600 mm^3, Z in [0, 8]).
namespace {
using namespace voelund;
using application::CommandStatus;
using document::DatumPlane;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
document::FeatureId committed(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Command was not committed: " + result.diagnostic());
    return *result.feature();
}
document::SketchParameters onDatum(DatumPlane datum, std::vector<document::SketchElement> e) {
    std::uint64_t next = 1;
    for (const auto& element : e)
        next = std::max(next, element.id.value() + 1);
    document::SketchParameters sketch{document::datumSketchPlane(datum),
                                      document::SketchElements{std::move(e), next}};
    sketch.datum = datum;
    return sketch;
}
document::SketchElement rectangle(double width, double height) {
    return {document::SketchElementId{1}, document::SketchRectangle{0, 0, width, height}};
}
Bnd_Box bounds(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    box.SetGap(0);
    return box;
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

// Selects every region of the sketch through the projection and extrudes them.
document::FeatureId extrude(application::CadSession& session, document::FeatureId sketch,
                            double distance) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select the outer region");
    const std::vector<document::SketchRegionId> regions(session.selectedSketchProfiles().begin(),
                                                        session.selectedSketchProfiles().end());
    return committed(session.createSketchProfileExtrusion(sketch, regions, distance));
}

void extrusionOnXy() {
    application::CadSession session;
    session.newDocument();
    const auto sketch =
        committed(session.createDatumSketchFeature(onDatum(DatumPlane::xy, {rectangle(20, 10)})));
    const auto extrusion = extrude(session, sketch, 8);
    require(session.currentBodies().size() == 1 &&
                session.currentBodies().front().bodyId == document::BodyId{sketch.value()},
            "Base-plane sketch opens one body with the sketch's identity");
    const auto box = bounds(session.currentShape());
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    require(near(test::volumeOf(session.currentShape()), 1600) && near(z0, 0) && near(z1, 8),
            "A-EXT-01: 1600 mm^3 with Z in [0, 8]");
    require(session.selectFeature(extrusion) &&
                session.editSelectedExtrusionParameters(
                           {5, {}, session.selectedExtrusionParameters().regions})
                        .status() == CommandStatus::committed &&
                near(test::volumeOf(session.currentShape()), 1000),
            "Distance change");
    require(session.undo().status() == CommandStatus::committed &&
                near(test::volumeOf(session.currentShape()), 1600) &&
                session.redo().status() == CommandStatus::committed &&
                near(test::volumeOf(session.currentShape()), 1000),
            "Undo/redo of the distance");

    const auto path = std::filesystem::temp_directory_path() / "voelund-datum-sketch.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    std::filesystem::remove(path);
    const auto states = reopened.featureStates();
    require(std::ranges::all_of(states,
                                [](const auto& s) {
                                    return s.status == application::FeatureRuntimeStatus::current;
                                }) &&
                near(test::volumeOf(reopened.currentShape()), 1000),
            "Reopened base-plane sketch is current, not a legacy repair case");
    const auto snapshot = reopened.identifiedSnapshot().snapshot();
    require(snapshot.sketchParameters(sketch).datum == DatumPlane::xy &&
                !snapshot.features().front().inputFeature,
            "Base plane identity lost on save/open");
    std::cout << "XY extrusion 1600, distance edit, undo/redo and save/open passed\n";
}

void ringAndOtherPlanes() {
    application::CadSession session;
    session.newDocument();
    const auto ring = committed(session.createDatumSketchFeature(onDatum(
        DatumPlane::xy,
        {rectangle(20, 10), {document::SketchElementId{2}, document::SketchCircle{{10, 5}, 2}}})));
    (void)extrude(session, ring, 8);
    require(near(test::volumeOf(session.currentShape()), (200 - 4 * std::numbers::pi) * 8),
            "Ring on XY");
    for (const auto datum : {DatumPlane::xz, DatumPlane::yz}) {
        const auto sketch =
            committed(session.createDatumSketchFeature(onDatum(datum, {rectangle(4, 3)})));
        (void)extrude(session, sketch, 2);
        const auto body =
            std::ranges::find(session.currentBodies(), document::BodyId{sketch.value()},
                              &features::EvaluatedBody::bodyId);
        require(body != session.currentBodies().end() && near(test::volumeOf(body->shape), 24),
                "Separate body per base-plane sketch");
        double x0, y0, z0, x1, y1, z1;
        bounds(body->shape).Get(x0, y0, z0, x1, y1, z1);
        const bool expected = datum == DatumPlane::xz
                                  ? near(x0, 0) && near(x1, 4) && near(y0, -2) && near(y1, 0) &&
                                        near(z0, 0) && near(z1, 3)
                                  : near(x0, 0) && near(x1, 2) && near(y0, 0) && near(y1, 4) &&
                                        near(z0, 0) && near(z1, 3);
        require(expected, "Base plane axes and extrusion direction");
    }
    require(session.currentBodies().size() == 3, "Three independent bodies");
    std::cout << "ring on XY, XZ/YZ axes and three separate bodies passed\n";
}

void editorAndRejections() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::rectangle);
    require(editor.setPreview({0, 0}, {6, 4}) && editor.confirmPreview().has_value(),
            "Draw a rectangle on XY");
    const auto sketch = committed(editor.tryAccept());
    require(session.identifiedSnapshot().snapshot().sketchParameters(sketch).datum ==
                DatumPlane::xy,
            "Editor accepts a base-plane sketch");

    auto shifted = onDatum(DatumPlane::xy, {rectangle(2, 2)});
    shifted.plane.originZ = 1;
    require(session.createDatumSketchFeature(shifted).status() == CommandStatus::rejected,
            "A datum sketch off its plane is rejected");
    auto unplaced = onDatum(DatumPlane::xz, {rectangle(2, 2)});
    unplaced.datum.reset();
    require(session.createDatumSketchFeature(unplaced).status() == CommandStatus::rejected,
            "A sketch without support needs a base plane");
    std::cout << "editor on XY and invalid base-plane sketches rejected passed\n";
}
} // namespace

int main() {
    try {
        extrusionOnXy();
        ringAndOtherPlanes();
        editorAndRejections();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
