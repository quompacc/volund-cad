#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// S06: the regular polygon and the slot keep their meaning under dimension changes. The
// polygon's vertices lie on a construction circle, so equal sides cannot fold into a rhombus;
// the slot's construction axis carries length and orientation, its caps the width. Picks in
// any order and direction work; degenerate picks and side counts change nothing.
namespace {
using namespace voelund;
using application::SketchAutoRelation;
using application::SketchCreationInput;
using application::SketchCreationResult;
using application::SketchCreationRole;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using document::SketchArc;
using document::SketchCircle;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }
double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<SketchEditorSession> editor;
    Fixture() {
        session.newDocument();
        editor = std::make_unique<SketchEditorSession>(session, document::DatumPlane::xy);
    }
    SketchCreationResult create(SketchDrawingTool tool, const SketchCreationInput& input) {
        editor->setTool(tool);
        auto result = editor->create(input);
        require(result.has_value(), "Created");
        return *result;
    }
    void refused(SketchDrawingTool tool, const SketchCreationInput& input,
                 const std::string& reason) {
        editor->setTool(tool);
        const auto before = editor->draftParameters();
        try {
            static_cast<void>(editor->create(input));
        } catch (const std::invalid_argument& error) {
            require(std::string(error.what()).find(reason) != std::string::npos,
                    "Expected \"" + reason + "\", got \"" + error.what() + "\"");
            require(editor->draftParameters() == before, "No change for " + reason);
            return;
        }
        throw std::runtime_error("Not refused: " + reason);
    }
    [[nodiscard]] const document::SketchElement& element(SketchElementId id) const {
        for (const auto& item : editor->elements())
            if (item.id == id)
                return item;
        throw std::runtime_error("No element");
    }
    template <typename Geometry> [[nodiscard]] Geometry get(SketchElementId id) const {
        return std::get<Geometry>(element(id).geometry);
    }
    [[nodiscard]] int freedom() const { return editor->lastSolveResult().degreesOfFreedom; }
    [[nodiscard]] bool solved() const {
        return editor->lastSolveResult().status == features::SketchSolveStatus::success;
    }
};

std::vector<SketchElementId> sides(const SketchCreationResult& result) {
    std::vector<SketchElementId> ids;
    for (const auto& item : result.elements)
        if (item.role == SketchCreationRole::polygonSide)
            ids.push_back(item.id);
    return ids;
}

// Joined, equal sides with every vertex on the circle and turning one way: regular.
void requireRegular(const Fixture& f, const SketchCreationResult& result, const char* what) {
    const auto circle = f.get<SketchCircle>(*result.find(SketchCreationRole::polygonCircle));
    const auto ids = sides(result);
    const auto first = f.get<SketchLine>(ids.front());
    const double side = distance(first.start, first.end);
    double turn = 0.0;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto line = f.get<SketchLine>(ids[index]);
        const auto next = f.get<SketchLine>(ids[(index + 1) % ids.size()]);
        require(near(line.end, next.start), std::string(what) + ": sides joined");
        require(near(distance(line.start, line.end), side), std::string(what) + ": equal sides");
        require(near(distance(line.start, circle.center), circle.radius),
                std::string(what) + ": vertex on the circle");
        const double cross = (line.end.x - line.start.x) * (next.end.y - next.start.y) -
                             (line.end.y - line.start.y) * (next.end.x - next.start.x);
        require(index == 0 || cross * turn > 0.0, std::string(what) + ": convex");
        turn = cross;
    }
}

void polygon() {
    Fixture f;
    const auto hexagon = f.create(SketchDrawingTool::polygon, {{2, 1}, {8, 1}});
    const auto circleId = *hexagon.find(SketchCreationRole::polygonCircle);
    require(hexagon.elements.size() == 7 && f.element(circleId).construction,
            "Six sides and a construction circle");
    require(near(f.get<SketchCircle>(circleId).center, {2, 1}) &&
                near(f.get<SketchCircle>(circleId).radius, 6),
            "The circle runs through the vertices around the picked center");
    require(f.solved() && f.freedom() == 4,
            "Free: center, circumradius and rotation (" + std::to_string(f.freedom()) + ")");
    requireRegular(f, hexagon, "drawn");
    require(f.editor->closedProfileCount() == 1, "The construction circle is no profile");

    // Moving one vertex keeps the polygon regular: a rhombus is not possible.
    const auto ids = sides(hexagon);
    const auto first = f.get<SketchLine>(ids.front());
    require(f.editor->addFixedReference({ids.front(), Position::start}, first.start) &&
                f.editor->addFixedReference({ids.front(), Position::end}, {4, 6}),
            "Fix two vertices elsewhere");
    requireRegular(f, hexagon, "after moving a vertex");
    require(near(f.get<SketchLine>(ids.front()).end, {4, 6}), "The vertex moved");

    Fixture square;
    const auto four = [&] {
        require(square.editor->setPolygonSides(4), "Four sides");
        return square.create(SketchDrawingTool::polygon, {{0, 0}, {-3, -4}});
    }();
    require(near(square.get<SketchLine>(sides(four).front()).start, {-3, -4}),
            "The first side starts at the picked vertex in any direction");
    require(square.editor->selectElement(*four.find(SketchCreationRole::polygonCircle)) &&
                square.editor->setSelectedCircleRadius(10),
            "The circumradius is a dimension");
    requireRegular(square, four, "square after the radius change");
    require(near(distance(square.get<SketchLine>(sides(four).front()).start,
                          square.get<SketchLine>(sides(four).front()).end),
                 10 * std::sqrt(2.0)),
            "Square side follows the circumradius");
    require(!square.editor->setSelectedCircleRadius(-2), "A negative radius is refused");
    requireRegular(square, four, "square after a refused radius");
}

void polygonCenterAndSides() {
    Fixture f;
    SketchCreationInput input{{0, 0}, {5, 0}};
    input.firstSnap = application::SketchSnap{application::SketchSnapKind::origin};
    const auto result = f.create(SketchDrawingTool::polygon, input);
    const auto ids = sides(result);
    require(f.editor->selectElement(ids.front()) && f.editor->setSelectedLineLength(3),
            "Side length is a dimension");
    const auto circle = f.get<SketchCircle>(*result.find(SketchCreationRole::polygonCircle));
    require(near(circle.center, {0, 0}) && near(circle.radius, 3),
            "The center stays on the origin; a hexagon's side equals its circumradius");
    requireRegular(f, result, "side length change");

    require(!f.editor->setPolygonSides(2) && !f.editor->setPolygonSides(65),
            "Side counts outside 3 to 64 are refused");
    const auto sixAgain = f.create(SketchDrawingTool::polygon, {{20, 0}, {25, 0}});
    require(sides(sixAgain).size() == 6, "A refused count keeps the current one");
    f.refused(SketchDrawingTool::polygon, {{30, 0}, {30, 0}}, "Eckpunkt liegt im Mittelpunkt");
}

struct Slot final {
    SketchElementId firstSide, secondCap, secondSide, firstCap, axis;
};
Slot slotOf(const SketchCreationResult& result) {
    return {*result.find(SketchCreationRole::slotFirstSide),
            *result.find(SketchCreationRole::slotSecondCap),
            *result.find(SketchCreationRole::slotSecondSide),
            *result.find(SketchCreationRole::slotFirstCap),
            *result.find(SketchCreationRole::slotAxis)};
}

double lineDistance(const SketchLine& line, SketchPoint point) {
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    return std::abs((point.x - line.start.x) * dy - (point.y - line.start.y) * dx) /
           std::hypot(dx, dy);
}

// Caps on the axis ends with one radius, both sides tangent to both caps and as long as the
// axis: a slot of that length and width.
void requireSlot(const Fixture& f, const Slot& s, double length, double radius, const char* what) {
    const auto axis = f.get<SketchLine>(s.axis);
    const auto first = f.get<SketchArc>(s.firstCap);
    const auto second = f.get<SketchArc>(s.secondCap);
    require(near(axis.start, first.center) && near(axis.end, second.center),
            std::string(what) + ": axis ends on the cap centers");
    require(near(distance(axis.start, axis.end), length), std::string(what) + ": length");
    require(near(first.radius, radius) && near(second.radius, radius),
            std::string(what) + ": width");
    for (const auto id : {s.firstSide, s.secondSide}) {
        const auto side = f.get<SketchLine>(id);
        require(near(lineDistance(side, first.center), radius) &&
                    near(lineDistance(side, second.center), radius),
                std::string(what) + ": side tangent to both caps");
        require(near(distance(side.start, side.end), length),
                std::string(what) + ": side as long as the axis");
    }
}

void slot() {
    Fixture f;
    const auto created = f.create(SketchDrawingTool::slot, {{0, 0}, {12, 0}, SketchPoint{0, 3}});
    const auto s = slotOf(created);
    require(created.elements.size() == 5 && f.element(s.axis).construction,
            "Four contour elements and a construction axis");
    require(f.solved() && f.freedom() == 5,
            "Free: both centers and the radius (" + std::to_string(f.freedom()) + ")");
    for (const auto& constraint : f.editor->constraints())
        require(constraint.kind != SketchConstraintKind::parallel,
                "No redundant parallel relation");
    requireSlot(f, s, 12, 3, "drawn");
    require(f.editor->closedProfileCount() == 1, "The axis is no profile");

    require(f.editor->selectElement(s.axis) && f.editor->setSelectedLineLength(20),
            "Length is the axis");
    require(f.editor->selectElement(s.firstCap) && f.editor->setSelectedCircleRadius(5),
            "Width is the cap radius");
    requireSlot(f, s, 20, 5, "after the length and width change");
    require(f.freedom() == 3, "Length and width fix two degrees of freedom");
    require(!f.editor->setSelectedCircleRadius(-1), "A negative width is refused");
    requireSlot(f, s, 20, 5, "after a refused width");

    // Picks from right to left at an angle, the width on the other side.
    Fixture reversed;
    const SketchPoint from{10, 4}, to{-2, 0};
    const double angle = std::atan2(to.y - from.y, to.x - from.x);
    const SketchPoint width{from.x + 2 * std::sin(angle), from.y - 2 * std::cos(angle)};
    const auto other = slotOf(reversed.create(SketchDrawingTool::slot, {from, to, width}));
    requireSlot(reversed, other, distance(from, to), 2, "reversed picks");
    require(near(reversed.get<SketchArc>(other.firstCap).center, from), "First cap at first pick");

    Fixture horizontal;
    SketchCreationInput input{{0, 0}, {12, 0}, SketchPoint{0, 3}};
    input.relation = SketchAutoRelation::horizontal;
    const auto level = slotOf(horizontal.create(SketchDrawingTool::slot, input));
    require(horizontal.freedom() == 4, "A horizontal slot has one freedom less");
    require(horizontal.editor->selectElement(level.firstCap) &&
                horizontal.editor->setSelectedCircleRadius(4) &&
                horizontal.editor->selectElement(level.axis) &&
                horizontal.editor->setSelectedLineLength(15),
            "Dimension the horizontal slot");
    const auto axis = horizontal.get<SketchLine>(level.axis);
    require(near(axis.start.y, axis.end.y), "The axis stays horizontal");
    requireSlot(horizontal, level, 15, 4, "horizontal");

    Fixture degenerate;
    degenerate.refused(SketchDrawingTool::slot, {{0, 0}, {10, 0}, SketchPoint{5, 0}},
                       "Breite ist null");
    degenerate.refused(SketchDrawingTool::slot, {{0, 0}, {0, 0}, SketchPoint{0, 3}},
                       "Mittelpunkte fallen zusammen");
}

} // namespace

int main() {
    try {
        polygon();
        polygonCenterAndSides();
        slot();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
