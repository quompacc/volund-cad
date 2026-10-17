#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// S04: every snap kind at the first and the second point of line, polyline and rectangle.
// The binding hangs on the picked point (first corner, second corner, line start/end), the
// sketch solves without conflict, no constraint is added twice, and the point sits where it
// was snapped.
namespace {
using namespace voelund;
using application::SketchCreationInput;
using application::SketchCreationPoint;
using application::SketchCreationResult;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using application::SketchSnap;
using application::SketchSnapKind;
using document::SketchPoint;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct Target final {
    const char* name;
    SketchSnap snap;
    SketchPoint at;
};

std::optional<SketchPoint> pointOf(const SketchEditorSession& editor,
                                   const document::SketchGeometryReference& reference) {
    const auto element =
        std::ranges::find(editor.elements(), reference.element, &document::SketchElement::id);
    if (element == editor.elements().end())
        return std::nullopt;
    if (const auto* line = std::get_if<document::SketchLine>(&element->geometry))
        return reference.point == document::SketchPointPosition::end ? line->end : line->start;
    return std::nullopt;
}

bool sameConstraint(const document::SketchConstraint& a, const document::SketchConstraint& b) {
    const bool sameRefs = (a.first == b.first && a.second == b.second) ||
                          (a.second && b.second && a.first == *b.second && *a.second == b.first);
    return a.kind == b.kind && sameRefs && a.third == b.third && a.value == b.value &&
           a.projectedEdge == b.projectedEdge;
}

// Every snap or relation constraint of the step removes a degree of freedom: without it the
// sketch has more. A consistent but redundant constraint does not show as a conflict.
void requireEffective(const SketchEditorSession& editor, const SketchCreationResult& result,
                      const std::string& name) {
    const int dof = editor.lastSolveResult().degreesOfFreedom;
    for (const auto& created : result.constraints) {
        if (created.origin == application::SketchConstraintOrigin::shape)
            continue;
        SketchEditorSession without = editor;
        require(without.removeConstraint(created.id), name + ": remove a constraint");
        require(without.lastSolveResult().degreesOfFreedom > dof,
                name + ": constraint #" + std::to_string(created.id.value()) + " is redundant");
    }
}

void check(SketchEditorSession editor, SketchDrawingTool tool, bool second, const Target& target,
           SketchPoint other) {
    const std::string name = std::string(tool == SketchDrawingTool::rectangle ? "rectangle"
                                         : tool == SketchDrawingTool::line    ? "line"
                                                                              : "polyline") +
                             (second ? " second " : " first ") + target.name;
    editor.setTool(tool);
    SketchCreationInput input{second ? other : target.at, second ? target.at : other};
    (second ? input.secondSnap : input.firstSnap) = target.snap;
    const std::size_t before = editor.constraints().size();
    std::optional<SketchCreationResult> result;
    try {
        result = editor.create(input);
    } catch (const std::exception& error) {
        throw std::runtime_error(name + ": " + error.what());
    }
    require(result.has_value(), name + ": nothing created");
    const auto role =
        tool == SketchDrawingTool::rectangle
            ? (second ? SketchCreationPoint::rectangleSecondCorner
                      : SketchCreationPoint::rectangleFirstCorner)
            : (second ? SketchCreationPoint::lineEnd : SketchCreationPoint::lineStart);
    const auto picked = result->point(role);
    require(picked.has_value(), name + ": the picked point has a role");
    bool bound = false;
    for (const auto& created : result->constraints) {
        if (created.origin != application::SketchConstraintOrigin::snap)
            continue;
        const auto constraint =
            std::ranges::find(editor.constraints(), created.id, &document::SketchConstraint::id);
        require(constraint != editor.constraints().end(), name + ": listed constraint exists");
        bound = bound || constraint->first == *picked ||
                (constraint->second && *constraint->second == *picked);
    }
    require(bound, name + ": the snap binds the picked point");
    const auto& solve = editor.lastSolveResult();
    require(solve.status == features::SketchSolveStatus::success &&
                solve.conflictingConstraints.empty(),
            name + ": solves without conflict");
    const auto constraints = editor.constraints();
    for (std::size_t a = before; a < constraints.size(); ++a)
        for (std::size_t b = 0; b < a; ++b)
            require(!sameConstraint(constraints[a], constraints[b]),
                    name + ": no constraint is added twice");
    requireEffective(editor, *result, name);
    const auto position = pointOf(editor, *picked);
    require(position && std::hypot(position->x - target.at.x, position->y - target.at.y) < 1e-6,
            name + ": the point sits on its snap");
}

void matrix() {
    application::CadSession session;
    session.newDocument();
    const auto box = session.createBoxFeature({40, 30, 20});
    require(box.status() == application::CommandStatus::committed, "Box");
    const auto support = session.features().front().id;
    const auto faces = session.supportFaces(support);
    const auto top = std::ranges::find_if(
        faces, [](const auto& face) { return std::abs(face.plane.normalZ - 1) < 1e-9; });
    require(top != faces.end(), "Top face");
    const auto sketch = session.sketchOnFace(support, top->origin);
    SketchEditorSession base(session, support, sketch.plane, *sketch.supportBinding);
    const auto projection = session.projectSketchSupport(support, *sketch.supportBinding);
    require(!projection.points.empty(), "Projected points");

    // Targets in the sketch: a line and an arc away from the axes and the box edges.
    base.setTool(SketchDrawingTool::line);
    require(base.setPreview({-60, 37}, {-44, 49}), "Target line");
    const auto line = *base.confirmPreview();
    base.setTool(SketchDrawingTool::arc);
    require(base.setPreview({-70, -40}, {-64, -40}, SketchPoint{-70, -34}), "Target arc");
    const auto arc = *base.confirmPreview();
    base.commitHistory();

    const auto& projected = projection.points.front();
    const std::vector<Target> targets{
        {"start", {SketchSnapKind::start, line}, {-60, 37}},
        {"end", {SketchSnapKind::end, line}, {-44, 49}},
        {"center", {SketchSnapKind::center, arc}, {-70, -40}},
        {"midpoint", {SketchSnapKind::midpoint, line}, {-52, 43}},
        {"on object", {SketchSnapKind::onObject, line}, {-56, 40}},
        {"origin", {SketchSnapKind::origin}, {0, 0}},
        {"x axis", {SketchSnapKind::xAxis}, {-23, 0}},
        {"y axis", {SketchSnapKind::yAxis}, {0, -27}},
        {"projected",
         {SketchSnapKind::projected, document::SketchElementId{0}, 0, 0, projected.reference},
         projected.local},
    };
    for (const auto tool :
         {SketchDrawingTool::line, SketchDrawingTool::polyline, SketchDrawingTool::rectangle})
        for (const bool second : {false, true})
            for (const auto& target : targets)
                check(base, tool, second, target,
                      {target.at.x + 13.0, target.at.y + 7.0}); // not axis-aligned
    std::cout << "PASS snap matrix: 9 kinds x first/second x line, polyline, rectangle\n";
}

// A horizontal line from the origin to the x axis: both snaps already make it horizontal, so
// the suggested horizontal relation must not be added on top (redundant).
void noRedundantRelation() {
    application::CadSession session;
    session.newDocument();
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::line);
    SketchCreationInput input{{0, 0}, {25, 0}};
    input.firstSnap = SketchSnap{SketchSnapKind::origin};
    input.secondSnap = SketchSnap{SketchSnapKind::xAxis};
    input.relation = application::SketchAutoRelation::horizontal;
    const auto result = editor.create(input);
    require(result.has_value(), "Line on the x axis");
    const auto& solve = editor.lastSolveResult();
    std::cout << "origin->x axis line: " << editor.constraints().size() << " constraints, status "
              << static_cast<int>(solve.status) << ", dof " << solve.degreesOfFreedom << '\n';
    require(solve.status == features::SketchSolveStatus::success &&
                solve.conflictingConstraints.empty(),
            "No conflict");
    requireEffective(editor, *result, "origin->x axis line");
}
// S04: a projected snap stores a typed edge reference. When that edge is gone, the sketch
// reports a reference error and keeps the reference; nothing rebinds to another edge.
void removedProjection() {
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature({40, 30, 20}).status() ==
                application::CommandStatus::committed,
            "Box");
    const auto box = session.features().front().id;
    const auto faces = session.supportFaces(box);
    const auto top = std::ranges::find_if(
        faces, [](const auto& face) { return std::abs(face.plane.normalZ - 1) < 1e-9; });
    const auto onTop = session.sketchOnFace(box, top->origin);
    SketchEditorSession editor(session, box, onTop.plane, *onTop.supportBinding);
    const auto projection = session.projectSketchSupport(box, *onTop.supportBinding);
    const auto& edgePoint = projection.points.front();
    editor.setTool(SketchDrawingTool::line);
    SketchCreationInput input{edgePoint.local, {edgePoint.local.x + 9, edgePoint.local.y + 4}};
    input.firstSnap = SketchSnap{SketchSnapKind::projected, document::SketchElementId{0}, 0, 0,
                                 edgePoint.reference};
    require(editor.create(input).has_value(), "Line from a projected edge point");
    const auto accepted = editor.tryAccept();
    require(accepted.status() == application::CommandStatus::committed && accepted.feature(),
            "Sketch with the projected snap");
    const auto sketchId = *accepted.feature();
    auto broken = session.sketchParameters(sketchId);
    const auto stored = std::ranges::find_if(broken.constraints.values, [&](const auto& c) {
        return c.projectedEdge == edgePoint.reference;
    });
    require(stored != broken.constraints.values.end(),
            "The snap is stored as a typed edge reference, not as coordinates");
    // The edge's generating feature is gone (a well-formed reference to nothing).
    std::get<document::PrimitiveEdgeOrigin>(stored->projectedEdge->origin.value).feature =
        document::FeatureId{999};
    const auto edited = session.editSketchParameters(
        sketchId, broken, application::ErrorDisposition::keepEditableErrors);
    require(edited.status() == application::CommandStatus::committedWithErrors,
            "A vanished edge is a reference error, got status " +
                std::to_string(static_cast<int>(edited.status())) + " reason " +
                std::to_string(static_cast<int>(edited.reason())) + ": " + edited.diagnostic());
    const auto states = session.featureStates();
    const auto state = std::ranges::find(states, sketchId, &application::FeatureRuntimeState::id);
    require(state != states.end() && state->status == application::FeatureRuntimeStatus::error,
            "The sketch shows the error");
    require(session.sketchParameters(sketchId) == broken,
            "The reference stays as it was; nothing rebinds");
}

// S04: with automatic relations off the step adds no relation of its own.
void relationsOff() {
    application::CadSession session;
    session.newDocument();
    SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(SketchDrawingTool::line);
    SketchCreationInput input{{3, 4}, {25, 4}};
    input.relation = application::SketchAutoRelation::horizontal;
    input.automaticRelations = false;
    require(editor.create(input).has_value() && editor.constraints().empty(),
            "No suggested relation without automatic relations");
    SketchCreationInput oblique{{3, 10}, {20, 21}};
    oblique.automaticRelations = false;
    require(editor.create(oblique).has_value() && editor.constraints().empty(),
            "No inferred relation without automatic relations");
}
} // namespace

int main() {
    try {
        matrix();
        noRedundantRelation();
        relationsOff();
        removedProjection();
        std::cout << "PASS S04 snaps\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
