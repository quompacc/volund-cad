// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// S11: a fillet between two lines. A right angle gives the analytic arc with tangent joints;
// the corner's dimensions move to a virtual corner, so a determined corner stays determined
// and keeps following its dimensions; what cannot keep its meaning is removed and listed;
// refusals (radius, parallel, curves) name the lines and change nothing; one undo step.
namespace {
using namespace voelund;
using application::SketchEditorSession;
using application::SketchEditRefusal;
using document::SketchArc;
using document::SketchCircle;
using document::SketchConstraint;
using document::SketchConstraintId;
using document::SketchConstraintKind;
using document::SketchElementId;
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
SketchGeometryReference ref(std::uint64_t element, Position point = Position::start) {
    return {SketchElementId{element}, point};
}
SketchConstraint relation(std::uint64_t id, Kind kind, SketchGeometryReference first,
                          std::optional<SketchGeometryReference> second = std::nullopt,
                          double value = 0.0) {
    return {SketchConstraintId{id}, kind, first, second, value};
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Fixture(const std::vector<document::SketchGeometry>& geometry,
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
    void select(std::uint64_t first, SketchPoint firstPick, std::uint64_t second,
                SketchPoint secondPick) {
        require(editor->selectNearestReference(SketchElementId{first}, firstPick, false) &&
                    editor->selectNearestReference(SketchElementId{second}, secondPick, true),
                "Select two lines");
    }
    application::SketchCurveEditResult fillet(double radius) {
        auto result = editor->filletCorner(radius);
        require(editor->lastSolveResult().status == features::SketchSolveStatus::success,
                "Solved after the fillet");
        editor->commitHistory();
        return result;
    }
    void refused(double radius, const std::string& reason, std::vector<std::uint64_t> elements) {
        const auto before = editor->draftParameters();
        try {
            static_cast<void>(editor->filletCorner(radius));
        } catch (const SketchEditRefusal& refusal) {
            require(std::string(refusal.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + refusal.what() + "\"");
            std::vector<std::uint64_t> named;
            for (const auto id : refusal.elements())
                named.push_back(id.value());
            require(named == elements, "The refusal names the lines for " + reason);
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
    template <typename Geometry> [[nodiscard]] Geometry get(std::uint64_t id) const {
        for (const auto& item : editor->elements())
            if (item.id == SketchElementId{id})
                return std::get<Geometry>(item.geometry);
        throw std::runtime_error("No element " + std::to_string(id));
    }
    [[nodiscard]] std::optional<SketchConstraintId> find(Kind kind) const {
        for (const auto& constraint : editor->constraints())
            if (constraint.kind == kind)
                return constraint.id;
        return std::nullopt;
    }
};

// Bottom side and right side of a 10 x 10 corner, fully determined.
Fixture determinedCorner() {
    return Fixture({SketchLine{{0, 0}, {10, 0}}, SketchLine{{10, 0}, {10, 10}}},
                   {relation(1, Kind::coincident, ref(1, Position::end), ref(2)),
                    relation(2, Kind::horizontal, ref(1)), relation(3, Kind::vertical, ref(2)),
                    relation(4, Kind::distance, ref(1), std::nullopt, 10),
                    relation(5, Kind::distance, ref(2), std::nullopt, 10),
                    relation(6, Kind::coordinateX, ref(1), std::nullopt, 0),
                    relation(7, Kind::coordinateY, ref(1), std::nullopt, 0)});
}

void rightAngle() {
    auto f = determinedCorner();
    require(f.editor->lastSolveResult().degreesOfFreedom == 0, "The corner starts determined");
    const auto before = f.editor->draftParameters();
    f.select(1, {9, 0}, 2, {10, 1});
    const auto result = f.fillet(2);
    require(result.created == SketchElementId{3} && result.removed.empty(),
            "An arc, nothing removed");
    require(near(f.get<SketchLine>(1).end, {8, 0}) && near(f.get<SketchLine>(2).start, {10, 2}),
            "The lines end where the arc touches them");
    const auto arc = f.get<SketchArc>(3);
    require(near(arc.center, {8, 2}) && near(arc.radius, 2) &&
                near(arc.endAngle - arc.startAngle, pi / 2),
            "Analytic quarter arc around (8, 2)");
    require(near(f.get<SketchLine>(4).end, {10, 0}) && near(f.get<SketchLine>(5).end, {10, 0}),
            "Construction lines meet at the virtual corner");
    const auto& solve = f.editor->lastSolveResult();
    require(solve.degreesOfFreedom == 0 && solve.redundantConstraints.empty(),
            "Still determined, nothing redundant (" + std::to_string(solve.degreesOfFreedom) + ")");

    // The dimensions keep working: radius 3, then the width 12 measured to the corner.
    require(f.editor->editDimension(*f.find(Kind::radius), 3), "Radius 3");
    f.editor->commitHistory();
    require(near(f.get<SketchArc>(3).center, {7, 3}) && near(f.get<SketchLine>(1).end, {7, 0}),
            "A larger radius keeps the corner");
    std::optional<SketchConstraintId> width;
    for (const auto& constraint : f.editor->constraints())
        if (constraint.kind == Kind::pointDistance &&
            constraint.first.element == SketchElementId{1})
            width = constraint.id;
    require(width && f.editor->editDimension(*width, 12), "The length became the width");
    f.editor->commitHistory();
    require(near(f.get<SketchLine>(5).end, {12, 0}) && near(f.get<SketchArc>(3).center, {9, 3}),
            "The corner follows the width");
    require(f.editor->undo() && f.editor->undo() && f.editor->undo() &&
                f.editor->draftParameters() == before,
            "Each step undoes on its own, the fillet as one");
}

void refusals() {
    auto f = determinedCorner();
    f.select(1, {9, 0}, 2, {10, 1});
    f.refused(20, "höchstens 9.999 mm (beide Linien sind zu kurz)", {1, 2});
    f.refused(-1, "größer als 0", {});

    Fixture shortOne({SketchLine{{0, 0}, {10, 0}}, SketchLine{{10, 0}, {10, 3}}});
    shortOne.select(1, {9, 0}, 2, {10, 1});
    shortOne.refused(5, "die zweite Linie ist zu kurz", {2});

    Fixture parallel({SketchLine{{0, 0}, {10, 0}}, SketchLine{{0, 5}, {10, 5}}});
    parallel.select(1, {5, 0}, 2, {5, 5});
    parallel.refused(1, "parallel", {1, 2});
    Fixture collinear({SketchLine{{0, 0}, {10, 0}}, SketchLine{{12, 0}, {20, 0}}});
    collinear.select(1, {5, 0}, 2, {15, 0});
    collinear.refused(1, "auf einer Geraden", {1, 2});

    Fixture curve({SketchLine{{0, 0}, {10, 0}}, SketchCircle{{5, 5}, 2}});
    curve.select(1, {5, 0}, 2, {7, 5});
    curve.refused(1, "UnsupportedCurve", {2});
}

void crossingAndLostRelations() {
    // Crossing lines keep the parts at their selected ends; the cut-off ends had relations.
    Fixture f({SketchLine{{-10, 0}, {10, 0}}, SketchLine{{0, -10}, {0, 10}}},
              {relation(1, Kind::coordinateX, ref(1), std::nullopt, -10),
               relation(2, Kind::equal, ref(1), ref(2))});
    f.select(1, {8, 0}, 2, {0, 8});
    const auto result = f.fillet(2);
    require(near(f.get<SketchLine>(1).start, {2, 0}) && near(f.get<SketchLine>(1).end, {10, 0}) &&
                near(f.get<SketchLine>(2).start, {0, 2}),
            "The quadrant of the selected ends is rounded");
    require(near(f.get<SketchArc>(3).center, {2, 2}), "Arc in that quadrant");
    require(f.editor->elements().size() == 3, "No corner of the lines, no virtual corner");
    require(result.removed.size() == 2 && result.removed[0].kind == Kind::coordinateX &&
                result.removed[1].kind == Kind::equal,
            "The cut-off end's coordinate and the equal length are removed and listed");

    // A fixed line keeps its far end and the corner as coordinates.
    Fixture fixed(
        {SketchLine{{0, 0}, {10, 0}}, SketchLine{{10, 0}, {10, 10}}},
        {{SketchConstraintId{1}, Kind::fixed, ref(1), std::nullopt, 0, std::nullopt, {0, 10, 0}},
         relation(2, Kind::coincident, ref(1, Position::end), ref(2))});
    fixed.select(1, {9, 0}, 2, {10, 1});
    const auto kept = fixed.fillet(2);
    require(kept.removed.empty(), "Nothing removed");
    int coordinates = 0;
    for (const auto& constraint : fixed.editor->constraints())
        if (constraint.kind == Kind::coordinateX || constraint.kind == Kind::coordinateY)
            ++coordinates;
    require(coordinates == 4 && !fixed.find(Kind::fixed), "Fixed became four coordinates");
    require(near(fixed.get<SketchLine>(1).start, {0, 0}) &&
                near(fixed.get<SketchLine>(4).end, {10, 0}),
            "Far end and corner stay where they were fixed");
}

// N14a: a line ending where an arc (centre (15; 0), radius 5) starts its quarter: the corner at
// (10; 0) is square. The fillet circle lies outside the arc, 1 from both.
Fixture lineAndArc(std::vector<SketchConstraint> constraints = {}) {
    return Fixture({SketchLine{{0, 0}, {10, 0}}, SketchArc{{15, 0}, 5, pi / 2, pi}},
                   std::move(constraints));
}

void curveCorners() {
    auto f =
        lineAndArc({relation(1, Kind::coincident, ref(1, Position::end), ref(2, Position::end)),
                    relation(2, Kind::radius, ref(2, Position::center), std::nullopt, 5)});
    f.select(1, {5, 0}, 2, {15, 5});
    const auto result = f.fillet(1);
    const double x = 15 - std::sqrt(35.0);
    require(near(f.get<SketchLine>(1).end, {x, 0}), "The line ends where the fillet touches it");
    const auto fillet = f.get<SketchArc>(3);
    require(near(fillet.center, {x, 1}) && near(fillet.radius, 1), "Fillet centre 1 from both");
    const auto arc = f.get<SketchArc>(2);
    require(near(arc.endAngle, std::atan2(1.0, -std::sqrt(35.0))) && near(arc.radius, 5),
            "The arc is cut back to the joint, its radius kept");
    require(result.removed.size() == 1 && result.removed[0].kind == Kind::coincident,
            "The corner's coincidence is removed and listed, the radius kept");

    // Two arcs meeting square at (0; 10); the fillet lies inside both.
    Fixture arcs({SketchArc{{0, 0}, 10, 0, pi / 2}, SketchArc{{10, 10}, 10, pi, 5 * pi / 4}});
    arcs.select(1, {7.07, 7.07}, 2, {1, 6});
    static_cast<void>(arcs.fillet(1));
    const double h = std::sqrt(15.5);
    require(near(arcs.get<SketchArc>(3).center, {5 - h, 5 + h}), "Centre 9 from both centres");
    require(near(arcs.get<SketchArc>(1).endAngle, std::atan2(5 + h, 5 - h)),
            "The first arc ends at the joint");

    // A chamfer 2 (straight) from the corner on both.
    auto c = lineAndArc();
    c.select(1, {5, 0}, 2, {15, 5});
    static_cast<void>(c.editor->chamferCorner(2));
    require(c.editor->lastSolveResult().status == features::SketchSolveStatus::success,
            "Solved after the chamfer");
    const auto line = c.get<SketchLine>(3);
    require(near(line.start, {8, 0}) && near(std::hypot(line.end.x - 10, line.end.y), 2),
            "Chamfer line between the two points 2 from the corner");

    auto big = lineAndArc();
    big.select(1, {5, 0}, 2, {15, 5});
    big.refused(50, "passt nicht", {1, 2});
    Fixture apart({SketchLine{{0, 0}, {9, 0}}, SketchArc{{15, 0}, 5, pi / 2, pi}});
    apart.select(1, {5, 0}, 2, {15, 5});
    apart.refused(1, "Endpunkt", {1, 2});
}

} // namespace

int main() {
    try {
        rightAngle();
        refusals();
        crossingAndLostRelations();
        curveCorners();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
