#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// S09: exact offsets. A line keeps the normal distance d, a circle becomes r +/- d, a closed
// rectangle and a slot grow outwards and shrink inwards with their joints kept closed, a
// collapsing inner offset is refused with its reason and changes nothing, an ellipse is
// declared unsupported, and the preview is the geometry the offset adds.
namespace {
using namespace voelund;
using application::SketchCreationInput;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using document::SketchElementId;
using document::SketchLine;
using document::SketchPoint;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Fixture() {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
    }
    std::vector<SketchElementId> create(SketchDrawingTool tool, SketchCreationInput input) {
        editor->setTool(tool);
        const auto result = editor->create(input);
        require(result.has_value(), "Created");
        std::vector<SketchElementId> ids;
        for (const auto& element : result->elements)
            ids.push_back(element.id);
        return ids;
    }
    void select(const std::vector<SketchElementId>& ids) {
        require(editor->selectElement(ids.front()), "Select");
        for (std::size_t index = 1; index < ids.size(); ++index)
            require(editor->selectNearestReference(ids[index], {0, 0}, true), "Extend selection");
    }
    // The elements the last offset added (they follow the selection's elements).
    std::vector<document::SketchGeometry> added(std::size_t count) const {
        std::vector<document::SketchGeometry> result;
        const auto elements = editor->elements();
        for (std::size_t index = elements.size() - count; index < elements.size(); ++index)
            result.push_back(elements[index].geometry);
        return result;
    }
    // A refused offset names its reason and leaves the editor as it was.
    void refused(double distance, const char* reason) {
        const auto before = editor->draftParameters();
        const auto problem = editor->offsetProblem(distance);
        require(problem && problem->find(reason) != std::string::npos,
                std::string("Expected \"") + reason + "\", got \"" + problem.value_or("") + "\"");
        require(!editor->offsetSelected(distance) && editor->draftParameters() == before,
                std::string("No change for ") + reason);
    }
};

void line() {
    Fixture f;
    const auto id = f.create(SketchDrawingTool::line, {{0, 0}, {10, 0}});
    f.select(id);
    require(f.editor->offsetSelected(2), "Offset a line");
    const auto copy = std::get<SketchLine>(f.added(1).front());
    require(near(copy.start, {0, 2}) && near(copy.end, {10, 2}),
            "A positive offset keeps distance 2 to the left of the line");
    f.select(id);
    require(f.editor->offsetSelected(-3), "Offset to the right");
    const auto right = std::get<SketchLine>(f.added(1).front());
    require(near(right.start, {0, -3}) && near(right.end, {10, -3}), "Distance 3 to the right");
    std::cout << "PASS line keeps the normal distance\n";
}

void circleAndArc() {
    Fixture f;
    const auto circle = f.create(SketchDrawingTool::circle, {{0, 0}, {5, 0}});
    f.select(circle);
    require(f.editor->offsetSelected(2) &&
                near(std::get<document::SketchCircle>(f.added(1).front()).radius, 7),
            "Circle r + d");
    f.select(circle);
    require(f.editor->offsetSelected(-2) &&
                near(std::get<document::SketchCircle>(f.added(1).front()).radius, 3),
            "Circle r - d");
    f.select(circle);
    f.refused(-5, "Der Kreis fällt");
    const auto arc = f.create(SketchDrawingTool::arc, {{20, 0}, {25, 0}, SketchPoint{20, 5}});
    f.select(arc);
    require(f.editor->offsetSelected(1) &&
                near(std::get<document::SketchArc>(f.added(1).front()).radius, 6),
            "A lone arc grows like a circle");
    std::cout << "PASS circle r +/- d, collapse refused, arc\n";
}

void rectangleContour() {
    Fixture f;
    const auto sides = f.create(SketchDrawingTool::rectangle, {{0, 0}, {20, 10}});
    f.editor->commitHistory();
    const auto start = f.editor->draftParameters();
    f.select(sides);
    // Preview and offset are the same geometry.
    require(f.editor->previewOffset(2), "Preview the outward offset");
    const auto withPreview = f.editor->evaluatedElements();
    std::vector<std::vector<SketchPoint>> previewed;
    for (std::size_t index = withPreview.size() - 4; index < withPreview.size(); ++index) {
        previewed.emplace_back();
        for (const auto& point : withPreview[index].points)
            previewed.back().push_back(point.local);
    }
    const std::size_t constraints = f.editor->constraints().size();
    require(f.editor->offsetSelected(2), "Offset the rectangle outwards");
    const auto evaluated = f.editor->evaluatedElements();
    for (std::size_t index = 0; index < 4; ++index) {
        const auto& points = evaluated[evaluated.size() - 4 + index].points;
        require(points.size() == previewed[index].size() &&
                    std::ranges::equal(points, previewed[index],
                                       [](const auto& a, SketchPoint b) { return near(a.local, b); }),
                "Preview and offset are geometrically identical");
    }
    std::vector<SketchPoint> corners;
    for (const auto& geometry : f.added(4))
        corners.push_back(std::get<SketchLine>(geometry).start);
    for (const SketchPoint corner :
         {SketchPoint{-2, -2}, SketchPoint{22, -2}, SketchPoint{22, 12}, SketchPoint{-2, 12}})
        require(std::ranges::any_of(corners, [&](SketchPoint c) { return near(c, corner); }),
                "Outer corner at distance 2");
    require(f.editor->constraints().size() == constraints + 4 &&
                f.editor->closedProfileCount() == 2,
            "Four joints keep the offset closed");
    f.editor->commitHistory();
    require(f.editor->undo() && f.editor->draftParameters() == start, "Undo is one step");

    f.select(sides);
    require(f.editor->offsetSelected(-2), "Offset the rectangle inwards");
    corners.clear();
    for (const auto& geometry : f.added(4))
        corners.push_back(std::get<SketchLine>(geometry).start);
    for (const SketchPoint corner :
         {SketchPoint{2, 2}, SketchPoint{18, 2}, SketchPoint{18, 8}, SketchPoint{2, 8}})
        require(std::ranges::any_of(corners, [&](SketchPoint c) { return near(c, corner); }),
                "Inner corner at distance 2");
    f.select(sides);
    f.refused(-5, "verschwindet ein Segment");
    f.refused(-7, "verschwindet ein Segment");
    std::cout << "PASS rectangle out and in, joints closed, preview = offset, collapse refused\n";
}

void slotContour() {
    Fixture f;
    auto slot = f.create(SketchDrawingTool::slot, {{0, 0}, {20, 0}, SketchPoint{0, 3}});
    slot.pop_back(); // the contour without the construction axis (S06)
    f.select(slot);
    require(f.editor->offsetSelected(1), "Offset the slot outwards");
    const auto grown = f.added(4);
    for (const auto& geometry : grown) {
        if (const auto* cap = std::get_if<document::SketchArc>(&geometry))
            require(near(cap->radius, 4), "Caps grow to radius 4");
        else
            require(near(std::abs(std::get<SketchLine>(geometry).start.y), 4),
                    "Sides move out to 4");
    }
    require(f.editor->closedProfileCount() == 2, "The offset slot is closed");
    f.select(slot);
    f.refused(-3, "Ein Bogen fällt");
    std::cout << "PASS slot offset with tangent joints\n";
}

void ellipseUnsupported() {
    Fixture f;
    const auto ellipse =
        f.create(SketchDrawingTool::ellipse, {{0, 0}, {10, 0}, SketchPoint{0, 4}});
    f.select(ellipse);
    f.refused(1, "UnsupportedCurve");
    std::cout << "PASS ellipse offset is declared unsupported\n";
}
} // namespace

int main() {
    try {
        line();
        circleAndArc();
        rectangleContour();
        slotContour();
        ellipseUnsupported();
        std::cout << "PASS S09 sketch offset\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
