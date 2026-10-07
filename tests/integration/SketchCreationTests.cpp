#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// U08: one creation path. For every drawing tool the editor's create() and the plain
// preview/confirm steps (plus the same explicit references) give the same editor state; the
// result names the created elements by role, and the roles point at the right geometry, so
// no caller needs IDs from insertion order. S01: every family names its points by role, arcs
// their direction, and the step lists the constraints it added; an unfit snap is an error.
namespace {
using namespace voelund;
using application::SketchCreationInput;
using application::SketchCreationPoint;
using application::SketchCreationResult;
using application::SketchCreationRole;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using application::SketchSnap;
using application::SketchSnapKind;
using document::SketchPoint;
using document::SketchPointPosition;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool same(const SketchEditorSession& a, const SketchEditorSession& b) {
    return std::ranges::equal(a.elements(), b.elements()) &&
           std::ranges::equal(a.constraints(), b.constraints());
}
bool near(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y) < 1e-9; }
const document::SketchGeometry& geometryOf(const SketchEditorSession& editor,
                                           document::SketchElementId id) {
    return std::ranges::find(editor.elements(), id, &document::SketchElement::id)->geometry;
}

struct Case final {
    const char* name;
    SketchDrawingTool tool;
    SketchCreationInput input;
};

// Without snaps, create() is exactly preview + confirm.
void plainStepsMatch(application::CadSession& session) {
    const Case cases[] = {
        {"line", SketchDrawingTool::line, {{0, 0}, {10, 3}}},
        {"rectangle", SketchDrawingTool::rectangle, {{1, 2}, {9, 7}}},
        {"circle", SketchDrawingTool::circle, {{2, 2}, {6, 2}}},
        {"arc", SketchDrawingTool::arc, {{0, 0}, {5, 0}, SketchPoint{0, 5}}},
        {"three-point arc", SketchDrawingTool::threePointArc, {{5, 0}, {0, 5}, SketchPoint{-5, 0}}},
        {"ellipse", SketchDrawingTool::ellipse, {{0, 0}, {8, 0}, SketchPoint{0, 3}}},
        {"slot", SketchDrawingTool::slot, {{0, 0}, {10, 0}, SketchPoint{0, 2}}},
        {"polygon", SketchDrawingTool::polygon, {{0, 0}, {6, 0}}},
    };
    for (const auto& item : cases) {
        SketchEditorSession viaCreate(session, document::DatumPlane::xy);
        SketchEditorSession viaSteps(session, document::DatumPlane::xy);
        viaCreate.setTool(item.tool);
        viaSteps.setTool(item.tool);
        // A free line gets its automatic relation in both paths.
        const auto created = viaCreate.create(item.input);
        require(viaSteps.setPreview(item.input.first, item.input.second, item.input.third),
                std::string(item.name) + ": preview");
        const auto confirmed = viaSteps.confirmCreation();
        if (item.tool == SketchDrawingTool::line)
            static_cast<void>(viaSteps.addAutomaticLineRelation(confirmed->primary()));
        // create() also lists the automatic relation of the free line; the shape is the same.
        require(created && confirmed && created->elements == confirmed->elements &&
                    created->points == confirmed->points &&
                    created->arcDirection == confirmed->arcDirection &&
                    same(viaCreate, viaSteps),
                std::string(item.name) + ": create() differs from the plain steps");
    }
}

// Roles point at geometry, not at positions in the element list.
void rolesNameGeometry(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::rectangle);
    const auto rectangle = editor.create({{1, 2}, {9, 7}});
    const auto fromSecond = rectangle->find(SketchCreationRole::rectangleFromSecondCorner);
    require(rectangle->elements.size() == 4 && fromSecond &&
                near(std::get<document::SketchLine>(geometryOf(editor, *fromSecond)).start, {9, 7}) &&
                near(std::get<document::SketchLine>(geometryOf(editor, rectangle->primary())).start,
                     {1, 2}),
            "Rectangle roles name the corner edges");
    editor.setTool(SketchDrawingTool::slot);
    const auto slot = editor.create({{20, 0}, {30, 0}, SketchPoint{20, 2}});
    const auto firstCap = slot->find(SketchCreationRole::slotFirstCap);
    const auto secondCap = slot->find(SketchCreationRole::slotSecondCap);
    require(firstCap && secondCap &&
                near(std::get<document::SketchArc>(geometryOf(editor, *firstCap)).center, {20, 0}) &&
                near(std::get<document::SketchArc>(geometryOf(editor, *secondCap)).center, {30, 0}),
            "Slot roles name the caps around each center");
}

// Snaps become the same references as the explicit editor calls.
void snapsMatchExplicitReferences(application::CadSession& session) {
    SketchEditorSession viaCreate(session, document::DatumPlane::xy);
    SketchEditorSession viaSteps(session, document::DatumPlane::xy);
    for (auto* editor : {&viaCreate, &viaSteps}) {
        editor->setTool(SketchDrawingTool::rectangle);
        static_cast<void>(editor->create({{0, 0}, {10, 6}}));
        editor->setTool(SketchDrawingTool::circle);
    }
    const auto corner = viaCreate.elements()[2].id; // Only to aim the snap; not a creation ID.
    SketchCreationInput input{{10, 6}, {13, 6}};
    input.firstSnap = SketchSnap{SketchSnapKind::start, corner};
    const auto circle = viaCreate.create(input);
    require(viaSteps.setPreview({10, 6}, {13, 6}), "circle preview");
    const auto stepCircle = viaSteps.confirmPreview();
    require(viaSteps.addCoincidentReferences({*stepCircle, SketchPointPosition::center},
                                             {corner, SketchPointPosition::start}),
            "explicit coincidence");
    require(circle && same(viaCreate, viaSteps),
            "A snapped circle center is the same coincidence as the explicit call");

    // Centered rectangle: rectangle plus two construction lines, centered on the snap.
    SketchEditorSession centered(session, document::DatumPlane::xy);
    centered.setTool(SketchDrawingTool::rectangle);
    SketchCreationInput around{{0, 0}, {4, 3}};
    around.centered = true;
    around.firstSnap = SketchSnap{SketchSnapKind::origin};
    const auto result = centered.create(around);
    const auto half = result->find(SketchCreationRole::centerHalfDiagonal);
    require(result->elements.size() == 6 && half &&
                result->find(SketchCreationRole::centerDiagonal) &&
                near(std::get<document::SketchLine>(geometryOf(centered, *half)).start, {0, 0}) &&
                centered.tool() == SketchDrawingTool::rectangle,
            "A centered rectangle names its construction lines");
}

// S01: points of the created geometry by role, after solving.
bool close(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y) < 1e-6; }
SketchPoint pointOf(const SketchEditorSession& editor, document::SketchGeometryReference reference) {
    const auto& geometry = geometryOf(editor, reference.element);
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return reference.point == SketchPointPosition::end ? line->end : line->start;
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        if (reference.point == SketchPointPosition::center)
            return arc->center;
        const double angle =
            reference.point == SketchPointPosition::start ? arc->startAngle : arc->endAngle;
        return {arc->center.x + arc->radius * std::cos(angle),
                arc->center.y + arc->radius * std::sin(angle)};
    }
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circle->center;
    return std::get<document::SketchEllipse>(geometry).center;
}
bool uniqueRoles(const SketchCreationResult& result) {
    std::set<std::pair<int, std::uint32_t>> elements;
    for (const auto& element : result.elements)
        if (!elements.insert({static_cast<int>(element.role), element.index}).second)
            return false;
    std::set<int> points;
    for (const auto& point : result.points)
        if (!points.insert(static_cast<int>(point.role)).second)
            return false;
    return true;
}
bool coincident(const SketchEditorSession& editor, document::SketchGeometryReference a,
                document::SketchGeometryReference b) {
    return std::ranges::any_of(editor.constraints(), [&](const document::SketchConstraint& item) {
        return item.kind == document::SketchConstraintKind::coincident && item.second &&
               ((item.first == a && *item.second == b) || (item.first == b && *item.second == a));
    });
}

// Every tool family names each of its points once, at the pick it came from.
void everyFamilyNamesItsPoints(application::CadSession& session) {
    using enum SketchCreationPoint;
    struct Expected final {
        SketchCreationPoint role;
        SketchPoint at;
    };
    struct Family final {
        const char* name;
        SketchDrawingTool tool;
        SketchCreationInput input;
        std::vector<Expected> points;
    };
    const Family families[] = {
        {"line", SketchDrawingTool::line, {{0, 0}, {10, 3}}, {{lineStart, {0, 0}}, {lineEnd, {10, 3}}}},
        {"polyline", SketchDrawingTool::polyline, {{1, 1}, {4, 5}},
         {{lineStart, {1, 1}}, {lineEnd, {4, 5}}}},
        {"rectangle", SketchDrawingTool::rectangle, {{1, 2}, {9, 7}},
         {{rectangleFirstCorner, {1, 2}}, {rectangleSecondCorner, {9, 7}}}},
        {"circle", SketchDrawingTool::circle, {{2, 2}, {6, 2}}, {{center, {2, 2}}}},
        {"arc", SketchDrawingTool::arc, {{0, 0}, {5, 0}, SketchPoint{0, 5}},
         {{center, {0, 0}}, {arcStart, {5, 0}}, {arcEnd, {0, 5}}}},
        {"three-point arc", SketchDrawingTool::threePointArc, {{5, 0}, {0, 5}, SketchPoint{-5, 0}},
         {{center, {0, 0}}, {arcStart, {5, 0}}, {arcEnd, {-5, 0}}}},
        {"ellipse", SketchDrawingTool::ellipse, {{0, 0}, {8, 0}, SketchPoint{0, 3}},
         {{center, {0, 0}}}},
        {"slot", SketchDrawingTool::slot, {{0, 0}, {10, 0}, SketchPoint{0, 2}},
         {{slotFirstCenter, {0, 0}}, {slotSecondCenter, {10, 0}}}},
        {"polygon", SketchDrawingTool::polygon, {{0, 0}, {6, 0}}, {{polygonVertex, {6, 0}}}},
    };
    for (const auto& family : families) {
        SketchEditorSession editor(session, document::DatumPlane::xy);
        editor.setTool(family.tool);
        const auto result = editor.create(family.input);
        const std::string name = family.name;
        require(result && uniqueRoles(*result) && result->points.size() == family.points.size(),
                name + ": unique roles");
        for (const auto& expected : family.points) {
            const auto reference = result->point(expected.role);
            require(reference && close(pointOf(editor, *reference), expected.at),
                    name + ": a point role names its pick");
        }
    }

    SketchEditorSession centered(session, document::DatumPlane::xy);
    centered.setTool(SketchDrawingTool::rectangle);
    SketchCreationInput around{{0, 0}, {4, 3}};
    around.centered = true;
    const auto result = centered.create(around);
    require(result && uniqueRoles(*result) &&
                close(pointOf(centered, *result->point(rectangleCenter)), {0, 0}) &&
                close(pointOf(centered, *result->point(rectangleSecondCorner)), {4, 3}) &&
                close(pointOf(centered, *result->point(rectangleFirstCorner)), {-4, -3}),
            "A centered rectangle names center and corners");

    // Polygon sides are numbered from the picked vertex on, each starting where the last ends.
    SketchEditorSession polygon(session, document::DatumPlane::xy);
    polygon.setTool(SketchDrawingTool::polygon);
    const auto sides = polygon.create({{0, 0}, {6, 0}});
    require(sides && sides->elements.size() == 6, "polygon sides");
    for (std::uint32_t index = 0; index < 6; ++index) {
        const auto side = sides->find(SketchCreationRole::polygonSide, index);
        const auto previous = sides->find(SketchCreationRole::polygonSide, (index + 5) % 6);
        require(side && previous &&
                    close(pointOf(polygon, {*side, SketchPointPosition::start}),
                          pointOf(polygon, {*previous, SketchPointPosition::end})) &&
                    (index != 0 ||
                     close(pointOf(polygon, {*side, SketchPointPosition::start}), {6, 0})),
                "Polygon side " + std::to_string(index) + " follows its predecessor");
    }
}

// A clockwise three-point arc is stored counterclockwise; its first pick is the stored end, and
// a snap at the first pick binds that end.
void reversedArcSwapsEnds(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::line);
    const auto anchor = editor.create({{-9, 0}, {-5, 0}});
    editor.setTool(SketchDrawingTool::threePointArc);
    SketchCreationInput clockwise{{-5, 0}, {0, 5}, SketchPoint{5, 0}};
    clockwise.firstSnap = SketchSnap{SketchSnapKind::end, anchor->primary()};
    const auto arc = editor.create(clockwise);
    const auto start = arc->point(SketchCreationPoint::arcStart);
    const auto end = arc->point(SketchCreationPoint::arcEnd);
    require(arc->arcDirection == application::SketchArcDirection::clockwise && start && end &&
                start->point == SketchPointPosition::end &&
                end->point == SketchPointPosition::start &&
                close(pointOf(editor, *start), {-5, 0}) && close(pointOf(editor, *end), {5, 0}),
            "A clockwise arc names its stored end as its start");
    require(coincident(editor, *start, {anchor->primary(), SketchPointPosition::end}),
            "The snap at the first pick binds the arc's stored end");

    SketchEditorSession forward(session, document::DatumPlane::xy);
    forward.setTool(SketchDrawingTool::threePointArc);
    const auto ccw = forward.create({{5, 0}, {0, 5}, SketchPoint{-5, 0}});
    require(ccw->arcDirection == application::SketchArcDirection::counterclockwise &&
                ccw->point(SketchCreationPoint::arcStart)->point == SketchPointPosition::start,
            "A counterclockwise arc keeps its stored start");
    forward.setTool(SketchDrawingTool::arc);
    require(forward.create({{20, 0}, {25, 0}, SketchPoint{20, 5}})->arcDirection ==
                application::SketchArcDirection::counterclockwise,
            "A center arc runs counterclockwise");
}

// Slot snaps bind the cap centers, also when the slot runs from right to left.
void slotSnapsUseCapCenters(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::circle);
    const auto circle = editor.create({{20, 0}, {21, 0}});
    editor.setTool(SketchDrawingTool::slot);
    SketchCreationInput input{{20, 0}, {10, 0}, SketchPoint{20, 2}};
    input.firstSnap = SketchSnap{SketchSnapKind::center, circle->primary()};
    const auto slot = editor.create(input);
    const auto first = slot->point(SketchCreationPoint::slotFirstCenter);
    require(first && first->point == SketchPointPosition::center &&
                first->element == *slot->find(SketchCreationRole::slotFirstCap) &&
                close(pointOf(editor, *first), {20, 0}) &&
                close(pointOf(editor, *slot->point(SketchCreationPoint::slotSecondCenter)),
                      {10, 0}),
            "Slot centers name the caps");
    require(coincident(editor, *first, {circle->primary(), SketchPointPosition::center}),
            "The snap binds the cap center");
}

// A snap whose target has no such point is an error, not a dropped or invalid constraint.
void unfitSnapIsAnError(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::line);
    const auto line = editor.create({{0, 0}, {10, 0}})->primary();
    editor.setTool(SketchDrawingTool::circle);
    const auto circle = editor.create({{20, 0}, {22, 0}})->primary();
    editor.setTool(SketchDrawingTool::arc);
    const auto arc = editor.create({{40, 0}, {45, 0}, SketchPoint{40, 5}})->primary();
    struct Unfit final {
        const char* name;
        SketchDrawingTool tool;
        SketchSnap snap;
    };
    const Unfit cases[] = {
        {"center of a line", SketchDrawingTool::circle, {SketchSnapKind::center, line}},
        {"end of a circle", SketchDrawingTool::line, {SketchSnapKind::end, circle}},
        {"midpoint of a circle", SketchDrawingTool::line, {SketchSnapKind::midpoint, circle}},
        {"on an arc", SketchDrawingTool::line, {SketchSnapKind::onObject, arc}},
        {"unknown element", SketchDrawingTool::line,
         {SketchSnapKind::start, document::SketchElementId{999}}},
    };
    for (const auto& item : cases) {
        SketchEditorSession attempt = editor;
        attempt.setTool(item.tool);
        SketchCreationInput input{{60, 0}, {63, 0}};
        input.firstSnap = item.snap;
        bool rejected = false;
        try {
            static_cast<void>(attempt.create(input));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, std::string("Snap to the ") + item.name + " is rejected");
    }
}

// The result lists exactly the constraints the step added, by origin.
void createdConstraintsAreListed(application::CadSession& session) {
    using application::SketchConstraintOrigin;
    const auto ids = [](const SketchEditorSession& editor) {
        std::set<std::uint64_t> result;
        for (const auto& constraint : editor.constraints())
            result.insert(constraint.id.value());
        return result;
    };
    const auto listed = [](const SketchCreationResult& result) {
        std::set<std::uint64_t> found;
        for (const auto& constraint : result.constraints)
            found.insert(constraint.id.value());
        return found;
    };
    const auto has = [](const SketchCreationResult& result, SketchConstraintOrigin origin) {
        return std::ranges::any_of(result.constraints,
                                   [origin](const auto& item) { return item.origin == origin; });
    };
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::rectangle);
    const auto rectangle = editor.create({{0, 0}, {10, 6}});
    require(rectangle->constraints.size() == 8 && listed(*rectangle) == ids(editor) &&
                !has(*rectangle, SketchConstraintOrigin::snap),
            "A rectangle lists its eight shape constraints");

    const auto corner = *rectangle->point(SketchCreationPoint::rectangleSecondCorner);
    editor.setTool(SketchDrawingTool::line);
    SketchCreationInput input{{10, 6}, {20, 12}};
    input.firstSnap = SketchSnap{SketchSnapKind::start, corner.element};
    const auto before = ids(editor);
    const auto snapped = editor.create(input);
    std::set<std::uint64_t> added;
    std::ranges::set_difference(ids(editor), before, std::inserter(added, added.end()));
    require(snapped && listed(*snapped) == added && has(*snapped, SketchConstraintOrigin::snap) &&
                !has(*snapped, SketchConstraintOrigin::shape),
            "A snapped line lists its snap constraint");

    SketchEditorSession free(session, document::DatumPlane::xy);
    free.setTool(SketchDrawingTool::line);
    static_cast<void>(free.create({{0, 0}, {10, 1}}));
    const auto parallel = free.create({{0, 5}, {10, 6.01}});
    require(parallel && has(*parallel, SketchConstraintOrigin::relation) &&
                parallel->constraints.size() == 1,
            "The automatic parallel relation is listed as a relation");
}
} // namespace

int main() {
    try {
        application::CadSession session;
        session.newDocument();
        plainStepsMatch(session);
        rolesNameGeometry(session);
        snapsMatchExplicitReferences(session);
        everyFamilyNamesItsPoints(session);
        reversedArcSwapsEnds(session);
        slotSnapsUseCapCenters(session);
        unfitSnapIsAnError(session);
        createdConstraintsAreListed(session);
        std::cout << "PASS U08/S01 one sketch creation path with semantic roles\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
