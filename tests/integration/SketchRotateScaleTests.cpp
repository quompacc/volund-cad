#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// S20 (user decision 2026-10-14): turning and scaling the selection. A 20 x 10 rectangle with
// a length 20 on its bottom turned 30 degrees about the origin has its corners where the
// rotation puts them, without horizontal/vertical relations; scaled by 2 about the origin its
// sides and the length double; factor 0 and a selection-free command are refused; undo; save
// and open.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::SketchConstraintKind;
using document::SketchPoint;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
bool has(const std::vector<SketchPoint>& points, SketchPoint point) {
    for (const auto& candidate : points)
        if (near(candidate.x, point.x) && near(candidate.y, point.y))
            return true;
    return false;
}
std::vector<SketchPoint> corners(const application::SketchEditorSession& editor) {
    std::vector<SketchPoint> points;
    for (const auto& element : editor.elements())
        if (const auto* line = std::get_if<document::SketchLine>(&element.geometry)) {
            points.push_back(line->start);
            points.push_back(line->end);
        }
    return points;
}
bool refused(const std::function<void()>& edit) {
    try {
        edit();
    } catch (const application::SketchEditRefusal&) {
        return true;
    }
    return false;
}

void rotateAndScale() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::rectangle);
    require(editor.setPreview({0, 0}, {20, 10}) && editor.confirmPreview().has_value(),
            "Rectangle");
    editor.commitHistory();
    std::vector<document::SketchElementId> sides;
    std::optional<document::SketchElementId> bottom;
    for (const auto& element : editor.elements()) {
        sides.push_back(element.id);
        const auto& line = std::get<document::SketchLine>(element.geometry);
        if (near(line.start.y, 0) && near(line.end.y, 0))
            bottom = element.id;
    }
    require(sides.size() == 4 && bottom, "Four sides");
    require(editor.selectNearestReference(*bottom, {0, 0}, false) &&
                editor.selectNearestReference(*bottom, {20, 0}, true) &&
                editor.addSelectedRelation(SketchConstraintKind::pointDistance),
            "Length 20 on the bottom");
    editor.commitHistory();

    editor.clearSelection();
    require(refused([&] { static_cast<void>(editor.rotateSelected({0, 0}, 1.0)); }),
            "Nothing selected: refused");
    require(editor.selectElements(sides, false), "All four sides selected");
    const auto center = editor.selectionCenter();
    require(center && near(center->x, 10) && near(center->y, 5), "Center of the selection");
    require(refused([&] { static_cast<void>(editor.scaleSelected({0, 0}, 0)); }),
            "Factor 0 refused");

    const double angle = std::numbers::pi / 6;
    const auto turned = editor.rotateSelected({0, 0}, angle);
    const double c = std::cos(angle), s = std::sin(angle);
    const auto points = corners(editor);
    for (const SketchPoint corner : {SketchPoint{0, 0}, SketchPoint{20, 0}, SketchPoint{20, 10},
                                     SketchPoint{0, 10}})
        require(has(points, {c * corner.x - s * corner.y, s * corner.x + c * corner.y}),
                "Corner turned by 30 degrees");
    bool axisBound = false;
    for (const auto& constraint : editor.constraints())
        axisBound = axisBound || constraint.kind == SketchConstraintKind::horizontal ||
                    constraint.kind == SketchConstraintKind::vertical;
    require(!axisBound && !turned.removed.empty() &&
                editor.lastSolveResult().status == features::SketchSolveStatus::success,
            "Horizontal and vertical relations removed and named");

    require(editor.selectElements(sides, false), "Selected again");
    static_cast<void>(editor.scaleSelected({0, 0}, 2));
    double length = 0;
    for (const auto& constraint : editor.constraints())
        if (constraint.kind == SketchConstraintKind::pointDistance)
            length = constraint.value;
    require(near(length, 40) && has(corners(editor), {40 * c, 40 * s}) &&
                has(corners(editor), {40 * c - 20 * s, 40 * s + 20 * c}),
            "Sides and the length double");

    require(editor.undo() && has(corners(editor), {20 * c, 20 * s}),
            "Undo returns to the turned rectangle");
    require(editor.redo(), "Redo the scale");
    require(editor.tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = session.features().back().id;

    const auto directory = std::filesystem::temp_directory_path() / "voelund-s20-transform";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "transform.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.sketchParameters(sketch) == session.sketchParameters(sketch),
            "Open restores the transformed sketch");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        rotateAndScale();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
