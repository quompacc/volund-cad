// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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

// S08: translate, rotate (circular pattern) and mirror for every geometry variant and the
// whole selection. Mirror copies survive a reallocating element vector (run it under a
// sanitizer where one is available); a legacy rectangle turned off its axes stays a rectangle
// as four constrained lines; a drawn rectangle can be patterned; an ellipse centre moves;
// delete takes the whole selection; undo restores IDs and relations.
namespace {
using namespace voelund;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using document::SketchElementId;
using document::SketchLine;
using document::SketchPoint;
using Kind = document::SketchConstraintKind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }

struct State final {
    std::vector<document::SketchElement> elements;
    std::vector<document::SketchConstraint> constraints;
    bool operator==(const State&) const = default;
};
State state(const SketchEditorSession& editor) {
    return {{editor.elements().begin(), editor.elements().end()},
            {editor.constraints().begin(), editor.constraints().end()}};
}
const document::SketchGeometry& geometry(const SketchEditorSession& editor, SketchElementId id) {
    const auto found = std::ranges::find(editor.elements(), id, &document::SketchElement::id);
    require(found != editor.elements().end(), "Element " + std::to_string(id.value()));
    return found->geometry;
}
SketchElementId addLine(SketchEditorSession& editor, SketchPoint first, SketchPoint second) {
    editor.setTool(SketchDrawingTool::line);
    require(editor.setPreview(first, second), "Line preview");
    const auto id = editor.confirmPreview();
    require(id.has_value(), "Line");
    return *id;
}
SketchPoint reflect(const SketchLine& axis, SketchPoint p) {
    const double dx = axis.end.x - axis.start.x, dy = axis.end.y - axis.start.y;
    const double t = ((p.x - axis.start.x) * dx + (p.y - axis.start.y) * dy) / (dx * dx + dy * dy);
    const SketchPoint foot{axis.start.x + t * dx, axis.start.y + t * dy};
    return {2 * foot.x - p.x, 2 * foot.y - p.y};
}
std::size_t count(const SketchEditorSession& editor, Kind kind) {
    return static_cast<std::size_t>(
        std::ranges::count(editor.constraints(), kind, &document::SketchConstraint::kind));
}
// Selects the elements in order; the last one is the mirror axis where it matters.
void selectAll(SketchEditorSession& editor, const std::vector<SketchElementId>& ids) {
    for (const auto id : ids) {
        const auto& shape = geometry(editor, id);
        SketchPoint at{0, 0};
        if (const auto* line = std::get_if<SketchLine>(&shape))
            at = line->start;
        else if (const auto* circle = std::get_if<document::SketchCircle>(&shape))
            at = circle->center;
        require(editor.selectNearestReference(id, at, id != ids.front()), "Select");
    }
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Fixture() {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
    }
};

// 20 lines mirrored at once: the old code kept a pointer to the axis inside the vector it
// appended to, and the copies after the first reallocation read freed memory.
void mirrorWithReallocation() {
    Fixture f;
    auto& editor = *f.editor;
    std::vector<SketchElementId> ids;
    for (int index = 0; index < 20; ++index)
        ids.push_back(addLine(editor, {5.0 + index, 2.0 + index}, {6.5 + index, 4.0 + 2 * index}));
    ids.push_back(addLine(editor, {-3.0, -40.0}, {-1.0, 60.0})); // oblique axis
    editor.commitHistory();
    const State before = state(editor);
    selectAll(editor, ids);
    require(editor.mirrorSelected(), "Mirror 20 lines");
    require(editor.elements().size() == 41, "20 mirror copies");
    const auto axis = std::get<SketchLine>(geometry(editor, ids.back()));
    for (std::size_t index = 0; index < 20; ++index) {
        const auto& source = std::get<SketchLine>(editor.elements()[index].geometry);
        const auto& copy = std::get<SketchLine>(editor.elements()[21 + index].geometry);
        require(near(copy.start, reflect(axis, source.start)) &&
                    near(copy.end, reflect(axis, source.end)),
                "Mirror copy " + std::to_string(index) + " is the reflection of its source");
    }
    editor.commitHistory();
    require(editor.undo() && state(editor) == before, "Undo restores IDs and relations");
    std::cout << "PASS mirror of 20 lines across a reallocation\n";
}

// A legacy rectangle mirrored at a 30-degree axis: four lines, closed, square-cornered. (At
// 45 degrees the mirror only swaps x and y, and the copy stays a rectangle.)
void legacyRectangle() {
    Fixture f;
    auto& editor = *f.editor;
    document::SketchElements elements{
        {{SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}},
         {SketchElementId{2}, SketchLine{{30, 0}, {30 + 10 * std::sqrt(3.0) / 2, 5}}}},
        3};
    require(editor.loadDraft(elements, {}), "Legacy rectangle");
    const State before = state(editor);
    // A whole-element pick, then the axis with Shift: both stay selected.
    require(editor.selectNearestReference(SketchElementId{1}, {0, 0}, false), "Rectangle");
    require(editor.selectNearestReference(SketchElementId{2}, {30, 0}, true), "Axis");
    require(editor.mirrorSelected(), "Mirror the rectangle at the oblique axis");
    require(editor.elements().size() == 6, "Four lines replace the rectangle copy");
    require(count(editor, Kind::coincident) == 4 && count(editor, Kind::perpendicular) == 1 &&
                count(editor, Kind::parallel) == 2,
            "The four lines are held as a rectangle");
    const auto axis = std::get<SketchLine>(elements.values[1].geometry);
    const auto& first = std::get<SketchLine>(editor.elements()[2].geometry);
    require(near(first.start, reflect(axis, {0, 0})) && near(first.end, reflect(axis, {20, 0})),
            "The first side is the reflected bottom edge");
    require(editor.closedProfileCount() == 2 && editor.canAccept(),
            "Original and copy are closed profiles");
    editor.commitHistory();
    require(editor.undo() && state(editor) == before, "Undo restores the legacy rectangle");

    // An axis-parallel mirror keeps a rectangle a rectangle.
    require(editor.loadDraft({{{SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}},
                               {SketchElementId{2}, SketchLine{{30, -5}, {30, 15}}}},
                              3},
                             {}),
            "Legacy rectangle and vertical axis");
    require(editor.selectNearestReference(SketchElementId{1}, {0, 0}, true) &&
                editor.selectNearestReference(SketchElementId{2}, {30, -5}, true) &&
                editor.mirrorSelected(),
            "Mirror at a vertical axis");
    const auto& mirrored = std::get<document::SketchRectangle>(editor.elements()[2].geometry);
    require(near(std::min(mirrored.first.x, mirrored.second.x), 40) &&
                near(std::max(mirrored.first.x, mirrored.second.x), 60),
            "The mirrored rectangle spans x 40..60");

    // A circular pattern in quarter turns keeps rectangles; in thirds it migrates them.
    require(
        editor.loadDraft({{{SketchElementId{1}, document::SketchRectangle{10, 0, 20, 5}}}, 2}, {}),
        "Legacy rectangle for the pattern");
    require(editor.selectNearestReference(SketchElementId{1}, {10, 0}, true) &&
                editor.circularPatternSelected(4, 360),
            "Pattern in quarter turns");
    require(editor.elements().size() == 4 &&
                std::ranges::all_of(editor.elements(),
                                    [](const auto& element) {
                                        return std::holds_alternative<document::SketchRectangle>(
                                            element.geometry);
                                    }),
            "Quarter turns keep four rectangles");
    require(editor.loadDraft({{{SketchElementId{1}, document::SketchRectangle{10, 0, 20, 5}}}, 2},
                             {}) &&
                editor.selectNearestReference(SketchElementId{1}, {10, 0}, true) &&
                editor.circularPatternSelected(3, 360),
            "Pattern in thirds");
    require(editor.elements().size() == 9 && count(editor, Kind::perpendicular) == 2,
            "Two copies as four constrained lines each");
    std::cout << "PASS legacy rectangle: oblique mirror and thirds migrate, axes keep it\n";
}

// A drawn rectangle is four lines marked horizontal/vertical; turned copies drop the marks.
void drawnRectanglePattern() {
    Fixture f;
    auto& editor = *f.editor;
    editor.setTool(SketchDrawingTool::rectangle);
    require(editor.setPreview({10, 0}, {20, 5}) && editor.confirmPreview().has_value(),
            "Rectangle");
    editor.commitHistory();
    const State before = state(editor);
    std::vector<SketchElementId> ids;
    for (const auto& element : editor.elements())
        ids.push_back(element.id);
    selectAll(editor, ids);
    require(editor.circularPatternSelected(3, 360), "Circular pattern of a drawn rectangle");
    require(editor.elements().size() == 12, "Two turned copies of four lines");
    for (std::size_t index = 4; index < 12; ++index)
        require(std::get<SketchLine>(editor.elements()[index].geometry).constraint ==
                    document::SketchLineConstraint::none,
                "A turned copy is not marked horizontal or vertical");
    editor.commitHistory();
    require(editor.undo() && state(editor) == before, "Undo restores the drawn rectangle");
    std::cout << "PASS drawn rectangle: circular pattern\n";
}

// Point and multiple selections move what they name; delete takes the whole selection.
void translateAndDelete() {
    Fixture f;
    auto& editor = *f.editor;
    editor.setTool(SketchDrawingTool::ellipse);
    require(editor.setPreview({0, 0}, {10, 0}, SketchPoint{0, 4}), "Ellipse preview");
    const auto ellipse = editor.confirmPreview();
    require(ellipse.has_value(), "Ellipse");
    require(editor.selectNearestReference(*ellipse, {0, 0}, false) &&
                editor.translateSelected(5, 2),
            "The ellipse centre moves");
    require(near(std::get<document::SketchEllipse>(geometry(editor, *ellipse)).center, {5, 2}),
            "Ellipse centre at (5, 2)");

    editor.setTool(SketchDrawingTool::circle);
    require(editor.setPreview({30, 0}, {33, 0}), "Circle preview");
    const auto circle = editor.confirmPreview();
    require(circle.has_value(), "Circle");
    const auto line = addLine(editor, {40, 10}, {50, 25});
    selectAll(editor, {*circle, line});
    require(editor.translateSelected(1, -1), "Move the circle and the line together");
    require(near(std::get<document::SketchCircle>(geometry(editor, *circle)).center, {31, -1}) &&
                near(std::get<SketchLine>(geometry(editor, line)).start, {41, 9}) &&
                near(std::get<SketchLine>(geometry(editor, line)).end, {51, 24}),
            "Both moved as a whole");

    editor.setTool(SketchDrawingTool::rectangle);
    require(editor.setPreview({60, 0}, {70, 5}) && editor.confirmPreview().has_value(),
            "Rectangle");
    editor.commitHistory();
    const State before = state(editor);
    const auto sides =
        std::vector<SketchElementId>({editor.elements()[editor.elements().size() - 4].id,
                                      editor.elements()[editor.elements().size() - 3].id});
    selectAll(editor, sides);
    require(editor.removeSelected(), "Delete two sides");
    require(editor.elements().size() == before.elements.size() - 2, "Both sides are gone");
    for (const auto& constraint : editor.constraints())
        for (const auto id : sides)
            require(constraint.first.element != id &&
                        (!constraint.second || constraint.second->element != id),
                    "No relation names a deleted side");
    editor.commitHistory();
    require(editor.undo() && state(editor) == before, "Undo restores both sides and relations");
    std::cout << "PASS ellipse centre, multiple translation, multiple delete\n";
}
} // namespace

int main() {
    try {
        mirrorWithReallocation();
        legacyRectangle();
        drawnRectanglePattern();
        translateAndDelete();
        std::cout << "PASS S08 sketch transforms\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
