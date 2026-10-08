#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// S10: trim, extend and split by curve type. Pieces are where the cuts are, relations on moved
// ends or on the old length are removed and named, the pieces stay on one line or circle, new
// ends are bound to their cutting edges, refusals change nothing, and each edit is one undo step.
namespace {
using namespace voelund;
using application::SketchCurveEdit;
using application::SketchEditorSession;
using document::SketchArc;
using document::SketchCircle;
using document::SketchConstraint;
using document::SketchConstraintId;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometry;
using document::SketchGeometryReference;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;
using Kind = SketchConstraintKind;

constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }
SketchElementId id(std::uint64_t value) { return SketchElementId{value}; }
SketchGeometryReference ref(std::uint64_t element, Position point = Position::start) {
    return {id(element), point};
}
SketchLine line(SketchPoint start, SketchPoint end) { return {start, end}; }

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    std::vector<SketchConstraint> relations;

    explicit Fixture(const std::vector<SketchGeometry>& geometry,
                     std::vector<SketchConstraint> constraints = {}) {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
        document::SketchElements elements;
        for (const auto& item : geometry)
            elements.values.push_back({SketchElementId{elements.nextId++}, item});
        document::SketchConstraints values{std::move(constraints), 0};
        values.nextId = values.values.size() + 1;
        require(editor->loadDraft(std::move(elements), std::move(values)), "Draft loads");
    }
    application::SketchCurveEditResult edit(SketchCurveEdit kind, std::uint64_t element,
                                            SketchPoint pick) {
        require(editor->selectNearestReference(id(element), pick, false), "Select");
        auto result = editor->editCurve(kind);
        require(editor->lastSolveResult().status == features::SketchSolveStatus::success,
                "Solved after the edit");
        editor->commitHistory();
        return result;
    }
    // A refused edit names its reason and leaves the editor as it was.
    void refused(SketchCurveEdit kind, std::uint64_t element, SketchPoint pick,
                 const std::string& reason) {
        const auto before = editor->draftParameters();
        require(editor->selectNearestReference(id(element), pick, false), "Select");
        try {
            static_cast<void>(editor->editCurve(kind));
        } catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + error.what() + "\"");
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
    [[nodiscard]] const SketchGeometry& geometry(std::uint64_t element) const {
        for (const auto& item : editor->elements())
            if (item.id == id(element))
                return item.geometry;
        throw std::runtime_error("No element " + std::to_string(element));
    }
    [[nodiscard]] SketchLine lineOf(std::uint64_t element) const {
        return std::get<SketchLine>(geometry(element));
    }
    [[nodiscard]] SketchArc arcOf(std::uint64_t element) const {
        return std::get<SketchArc>(geometry(element));
    }
    [[nodiscard]] bool has(Kind kind, SketchGeometryReference first,
                           SketchGeometryReference second) const {
        for (const auto& constraint : editor->constraints())
            if (constraint.kind == kind && constraint.first.element == first.element &&
                constraint.second && constraint.second->element == second.element &&
                (kind == Kind::collinear || kind == Kind::parallel || kind == Kind::concentric ||
                 kind == Kind::equal ||
                 (constraint.first.point == first.point &&
                  (kind == Kind::pointOnObject || constraint.second->point == second.point))))
                return true;
        return false;
    }
    [[nodiscard]] bool hasKind(Kind kind, std::uint64_t element) const {
        for (const auto& constraint : editor->constraints())
            if (constraint.kind == kind && constraint.first.element == id(element))
                return true;
        return false;
    }
};

SketchConstraint relation(std::uint64_t number, Kind kind, SketchGeometryReference first,
                          std::optional<SketchGeometryReference> second = std::nullopt,
                          double value = 0.0) {
    return {SketchConstraintId{number}, kind, first, second, value};
}

std::vector<Kind> kinds(const application::SketchCurveEditResult& result) {
    std::vector<Kind> values;
    for (const auto& constraint : result.removed)
        values.push_back(constraint.kind);
    return values;
}

void trimLineAtEnd() {
    Fixture f({line({0, 0}, {10, 0}), line({5, -5}, {5, 5})},
              {relation(1, Kind::distance, ref(1), std::nullopt, 10),
               relation(2, Kind::horizontal, ref(1)),
               relation(3, Kind::coordinateX, ref(1, Position::end), std::nullopt, 10)});
    const auto before = f.editor->draftParameters();
    const auto result = f.edit(SketchCurveEdit::trim, 1, {8, 0});
    require(near(f.lineOf(1).start, {0, 0}) && near(f.lineOf(1).end, {5, 0}),
            "The picked piece right of the cut is gone");
    require(kinds(result) == std::vector{Kind::distance, Kind::coordinateX},
            "Length and the moved end's coordinate are removed and named");
    require(f.hasKind(Kind::horizontal, 1), "The direction stays");
    require(f.has(Kind::pointOnObject, ref(1, Position::end), ref(2)),
            "The new end lies on the cutting line");
    require(!f.editor->selectedElement(), "Nothing stays selected under the removed piece");
    require(f.editor->undo() && f.editor->draftParameters() == before, "Undo is one step");
}

void trimLineInTheMiddle() {
    Fixture f({line({0, 0}, {10, 0}), line({3, -5}, {3, 5}), line({7, -5}, {7, 5}),
               line({10, 0}, {10, 5})},
              {relation(1, Kind::coincident, ref(1, Position::end), ref(4, Position::start)),
               relation(2, Kind::distance, ref(1), std::nullopt, 10)});
    const auto result = f.edit(SketchCurveEdit::trim, 1, {5, 0});
    require(result.created == id(5), "The right piece is a new element");
    require(near(f.lineOf(1).start, {0, 0}) && near(f.lineOf(1).end, {3, 0}) &&
                near(f.lineOf(5).start, {7, 0}) && near(f.lineOf(5).end, {10, 0}),
            "Only the piece between the cuts is gone; the end touching line 4 is no cut");
    require(f.has(Kind::coincident, ref(5, Position::end), ref(4, Position::start)),
            "The new piece takes over the end point with its relation");
    require(kinds(result) == std::vector{Kind::distance}, "The old length is removed");
    require(f.has(Kind::collinear, ref(5), ref(1)), "The pieces stay on one line");
    require(f.has(Kind::pointOnObject, ref(1, Position::end), ref(2)) &&
                f.has(Kind::pointOnObject, ref(5, Position::start), ref(3)),
            "Both new ends lie on their cutting lines");
}

void trimChoosesCuts() {
    Fixture onEnd({line({0, 0}, {10, 0}), line({5, 0}, {5, 5})});
    onEnd.edit(SketchCurveEdit::trim, 1, {8, 0});
    require(onEnd.has(Kind::coincident, ref(1, Position::end), ref(2, Position::start)),
            "A cut at the cutter's end point becomes coincident");

    Fixture touching({line({0, 0}, {5, 0}), line({5, -5}, {5, 5})});
    touching.refused(SketchCurveEdit::trim, 1, {2, 0}, "schneidet keine andere Geometrie");
    Fixture onCut({line({0, 0}, {10, 0}), line({5, -5}, {5, 5})});
    onCut.refused(SketchCurveEdit::trim, 1, {5, 0}, "Pickstelle liegt auf einem Schnittpunkt");

    Fixture rectangle({line({0, 0}, {10, 0}), document::SketchRectangle(3, -2, 7, 2)});
    const auto result = rectangle.edit(SketchCurveEdit::trim, 1, {5, 0});
    require(result.created == id(3) && near(rectangle.lineOf(1).end, {3, 0}) &&
                near(rectangle.lineOf(3).start, {7, 0}) && result.added.size() == 1,
            "Rectangle edges cut; the pieces only stay collinear (no point on a rectangle)");
    Fixture ellipse({line({0, 0}, {10, 0}), document::SketchEllipse{{5, 0}, 2, 1, 0}});
    ellipse.refused(SketchCurveEdit::trim, 1, {5, 0}, "schneidet keine andere Geometrie");
}

void trimAtTangent() {
    Fixture f({line({-10, 5}, {10, 5}), SketchCircle{{0, 0}, 5}});
    f.edit(SketchCurveEdit::trim, 1, {6, 5});
    require(near(f.lineOf(1).end, {0, 5}), "The tangent point is one cut");
    require(f.has(Kind::pointOnObject, ref(1, Position::end), ref(2)), "Bound to the circle");
}

void trimCircleAndArc() {
    Fixture f({SketchCircle{{0, 0}, 5}, line({-10, 0}, {10, 0}), line({0, -5}, {0, -10})},
              {relation(1, Kind::radius, ref(1, Position::center), std::nullopt, 5),
               relation(2, Kind::pointOnObject, ref(3), ref(1))});
    const auto result = f.edit(SketchCurveEdit::trim, 1, {0, 5});
    const auto arc = f.arcOf(1);
    require(near(arc.radius, 5) && near(arc.startAngle, pi) && near(arc.endAngle, 2 * pi),
            "The upper half is gone; the circle is the lower arc");
    require(f.hasKind(Kind::radius, 1), "The radius stays");
    require(kinds(result) == std::vector{Kind::pointOnObject},
            "A point on the circle cannot stay on an arc and is named");
    require(f.has(Kind::pointOnObject, ref(1, Position::start), ref(2)) &&
                f.has(Kind::pointOnObject, ref(1, Position::end), ref(2)),
            "Both arc ends lie on the line");

    Fixture once({SketchCircle{{0, 0}, 5}, line({-10, 5}, {10, 5})});
    once.refused(SketchCurveEdit::trim, 1, {0, -5}, "mindestens zwei Schnittpunkte");

    Fixture g({SketchArc{{0, 0}, 5, 0, pi}, line({0, 0}, {0, 10}), line({5, 0}, {10, 0})},
              {relation(1, Kind::coincident, ref(1, Position::start), ref(3, Position::start))});
    const auto arcResult = g.edit(SketchCurveEdit::trim, 1, {3.5, 3.5});
    require(near(g.arcOf(1).startAngle, pi / 2) && near(g.arcOf(1).endAngle, pi),
            "The arc keeps the part beyond the cut");
    require(kinds(arcResult) == std::vector{Kind::coincident}, "The moved start lets go");
    require(g.has(Kind::pointOnObject, ref(1, Position::start), ref(2)), "Bound to the line");
}

void extend() {
    Fixture f({line({0, 0}, {4, 0}), line({8, -2}, {8, 2}), SketchCircle{{-10, 0}, 3}},
              {relation(1, Kind::distance, ref(1), std::nullopt, 4)});
    const auto result = f.edit(SketchCurveEdit::extend, 1, {3.5, 0});
    require(near(f.lineOf(1).start, {0, 0}) && near(f.lineOf(1).end, {8, 0}),
            "The end nearest the pick reaches the line");
    require(kinds(result) == std::vector{Kind::distance}, "The old length is removed");
    require(f.has(Kind::pointOnObject, ref(1, Position::end), ref(2)), "Bound to the line");
    f.edit(SketchCurveEdit::extend, 1, {0.5, 0});
    require(near(f.lineOf(1).start, {-7, 0}), "The start reaches the nearer side of the circle");
    f.refused(SketchCurveEdit::extend, 1, {7.5, 0}, "keine Geometrie");

    Fixture miss({line({0, 0}, {4, 0}), line({8, 1}, {8, 5})});
    miss.refused(SketchCurveEdit::extend, 1, {3.5, 0}, "keine Geometrie");

    Fixture arc({SketchArc{{0, 0}, 5, 0, pi / 2}, line({-3, -10}, {-3, 10})});
    arc.edit(SketchCurveEdit::extend, 1, {0.5, 5});
    require(near(arc.arcOf(1).startAngle, 0) &&
                near(arc.arcOf(1).endAngle, std::atan2(4.0, -3.0)),
            "The arc end runs along its circle to the line");
    require(arc.has(Kind::pointOnObject, ref(1, Position::end), ref(2)), "Bound to the line");
}

void split() {
    Fixture f({line({0, 0}, {10, 0}), line({10, 0}, {10, 5})},
              {relation(1, Kind::distance, ref(1), std::nullopt, 10),
               relation(2, Kind::coincident, ref(1, Position::end), ref(2, Position::start))});
    const auto before = f.editor->draftParameters();
    const auto result = f.edit(SketchCurveEdit::split, 1, {4, 0});
    require(result.created == id(3) && result.removed.empty(), "A new piece, nothing removed");
    require(near(f.lineOf(1).end, {4, 0}) && near(f.lineOf(3).start, {4, 0}) &&
                near(f.lineOf(3).end, {10, 0}),
            "Split at the picked point");
    require(f.editor->constraintValue(SketchConstraintId{1}) &&
                near(*f.editor->constraintValue(SketchConstraintId{1}), 4),
            "The length dimension keeps the first piece's length");
    require(f.has(Kind::coincident, ref(3, Position::end), ref(2, Position::start)) &&
                f.has(Kind::coincident, ref(1, Position::end), ref(3, Position::start)) &&
                f.has(Kind::parallel, ref(3), ref(1)),
            "The end moves to the new piece; the pieces stay joined in one direction");
    require(f.editor->selectedElement() == id(3), "The new piece is selected");
    require(f.editor->undo() && f.editor->draftParameters() == before, "Undo is one step");

    Fixture arc({SketchArc{{0, 0}, 5, 0, pi}});
    arc.edit(SketchCurveEdit::split, 1, {0, 5});
    require(near(arc.arcOf(1).endAngle, pi / 2) && near(arc.arcOf(2).startAngle, pi / 2) &&
                near(arc.arcOf(2).endAngle, pi),
            "An arc splits into two arcs");
    require(arc.has(Kind::concentric, ref(2), ref(1)) &&
                arc.has(Kind::coincident, ref(1, Position::end), ref(2, Position::start)),
            "The arcs stay on one circle");

    Fixture end({line({0, 0}, {10, 0})});
    end.refused(SketchCurveEdit::split, 1, {0.0005, 0}, "Nullsegment");
}

void unsupported() {
    Fixture f({SketchCircle{{0, 0}, 5}, document::SketchEllipse{{20, 0}, 4, 2, 0},
               line({-10, 0}, {30, 0})});
    require(f.editor->selectNearestReference(id(1), {0, 5}, false), "Select the circle");
    require(f.editor->canEditCurve(SketchCurveEdit::trim) &&
                !f.editor->canEditCurve(SketchCurveEdit::extend) &&
                !f.editor->canEditCurve(SketchCurveEdit::split),
            "A circle can only be trimmed");
    f.refused(SketchCurveEdit::extend, 1, {0, 5}, "UnsupportedCurve");
    f.refused(SketchCurveEdit::split, 1, {0, 5}, "UnsupportedCurve");
    require(f.editor->selectNearestReference(id(2), {20, 2}, false) &&
                !f.editor->canEditCurve(SketchCurveEdit::trim),
            "An ellipse cannot be trimmed");
    f.refused(SketchCurveEdit::trim, 2, {20, 2}, "Ellipsen werden nicht unterstützt");
}

} // namespace

int main() {
    try {
        trimLineAtEnd();
        trimLineInTheMiddle();
        trimChoosesCuts();
        trimAtTangent();
        trimCircleAndArc();
        extend();
        split();
        unsupported();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
