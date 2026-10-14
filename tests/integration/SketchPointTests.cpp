#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// S15 (user decision 2026-10-14): a single point in a sketch. It is construction geometry and
// bounds no region; a locked point leaves no freedom; on a line it follows the line; a point
// distance drives it; curve edits refuse it; it is saved and opened (schema 22).
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
const document::SketchPointGeometry& pointOf(const application::SketchEditorSession& editor,
                                             std::uint64_t id) {
    for (const auto& element : editor.elements())
        if (element.id.value() == id)
            return std::get<document::SketchPointGeometry>(element.geometry);
    throw std::runtime_error("Point missing");
}

void points() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::point);
    require(editor.setPreview({5, 7}, {5, 7}) &&
                editor.confirmPreview() == document::SketchElementId{1},
            "A point is placed with one pick");
    require(editor.elements().front().construction, "A point is construction geometry");

    // A closed rectangle of lines around a second point: one region, the point bounds none.
    editor.setTool(application::SketchDrawingTool::line);
    const document::SketchPoint corners[]{{0, 0}, {20, 0}, {20, 10}, {0, 10}};
    for (int index = 0; index < 4; ++index)
        require(editor.setPreview(corners[index], corners[(index + 1) % 4]) &&
                    editor.confirmPreview().has_value(),
                "Rectangle side");
    require(features::identifySketchRegions(editor.elements()).ids.size() == 1,
            "The point bounds no region");

    // The point on the bottom line, 4 from its start.
    require(editor.selectNearestReference(document::SketchElementId{1}, {5, 7}, false) &&
                editor.selectNearestReference(document::SketchElementId{2}, {10, 0}, true) &&
                editor.canAddSelectedRelation(document::SketchConstraintKind::pointOnObject) &&
                editor.addSelectedRelation(document::SketchConstraintKind::pointOnObject),
            "Point on the line");
    const auto& bottom = std::get<document::SketchLine>(editor.elements()[1].geometry);
    const auto& at = pointOf(editor, 1).position;
    require(near((bottom.end.x - bottom.start.x) * (at.y - bottom.start.y) -
                     (bottom.end.y - bottom.start.y) * (at.x - bottom.start.x),
                 0),
            "The point lies on the line");
    require(editor.selectNearestReference(document::SketchElementId{1}, pointOf(editor, 1).position,
                                          false) &&
                editor.selectNearestReference(document::SketchElementId{2}, bottom.start, true) &&
                editor.addSelectedRelation(document::SketchConstraintKind::pointDistance) &&
                editor.setConstraintValue(editor.constraints().back().id, 4),
            "Distance 4 from the line start");
    const auto& line = std::get<document::SketchLine>(editor.elements()[1].geometry);
    require(near(std::hypot(pointOf(editor, 1).position.x - line.start.x,
                            pointOf(editor, 1).position.y - line.start.y),
                 4),
            "The distance drives the point");

    // Curve edits refuse a point.
    require(editor.selectElement(document::SketchElementId{1}) &&
                !editor.canEditCurve(application::SketchCurveEdit::trim) &&
                !editor.canEditCurve(application::SketchCurveEdit::split),
            "A point is no curve");

    // A free point, locked: no freedom left in it.
    editor.setTool(application::SketchDrawingTool::point);
    require(editor.setPreview({30, 30}, {30, 30}) && editor.confirmPreview().has_value(),
            "Second point");
    const auto free = editor.elements().back().id;
    require(editor.selectNearestReference(free, {30, 30}, false) &&
                editor.lockSelectedReference(),
            "Lock the point");
    require(std::ranges::find(editor.lastSolveResult().freeElements, free) ==
                editor.lastSolveResult().freeElements.end(),
            "A locked point is determined");

    require(editor.tryAccept().status() == application::CommandStatus::committed,
            "The sketch is accepted");
    const auto sketch = session.features().back().id;
    const auto directory = std::filesystem::temp_directory_path() / "voelund-s15-point";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "point.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.sketchParameters(sketch) == session.sketchParameters(sketch),
            "Open restores the points");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        points();
        std::cout << "PASS S15 sketch point: one pick, construction, no region, on a line, "
                     "distance, no curve edit, locked, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
