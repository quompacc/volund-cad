#include <BRepBuilderAPI_MakeEdge.hxx>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

using namespace voelund;
using namespace features;
namespace {
using Role = kernel::PrimitiveRole;
const document::FeatureId source{1};
const PrimitiveEdgeOrigin topX{source, Role::boxNegativeY, Role::boxPositiveZ};
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double value, double expected) {
    require(std::abs(value - expected) < 1e-7, "Analytic point mismatch");
}
gp_Pnt point(std::span<const EdgeProvenance> origins, const PrimitiveEdgeOrigin& key, double t) {
    const auto result = resolveEdgePoint(origins, key, {EdgePointKind::parameter, t});
    require(result.status == EdgeResolutionStatus::resolved && result.point.has_value(),
            "Expected resolved edge point");
    return *result.point;
}
const EdgeProvenance& edge(std::span<const EdgeProvenance> origins,
                           const PrimitiveEdgeOrigin& key) {
    const auto found = std::ranges::find(origins, key, &EdgeProvenance::origin);
    require(found != origins.end(), "Missing edge origin");
    return *found;
}
const EvaluatedFeature& output(const FeatureGraphEvaluation& result, document::FeatureId id) {
    const auto found = std::ranges::find(result.result.features, id, &EvaluatedFeature::id);
    require(found != result.result.features.end(), "Missing feature output");
    return *found;
}
void primitivesAndDisplay() {
    const auto box = kernel::ShapeFactory::makeBoxWithHistory({10, 20, 30});
    auto origins = primitiveEdgeOrigins(source, box);
    require(origins.size() == 12, "Box edges not uniquely named");
    const auto start = resolveEdgePoint(origins, topX, {EdgePointKind::start});
    const auto end = resolveEdgePoint(origins, topX, {EdgePointKind::end});
    require(start.point && end.point, "Missing endpoints");
    near(start.point->X(), 0);
    near(end.point->X(), 10);
    near(start.point->Y(), 0);
    near(start.point->Z(), 30);
    near(point(origins, topX, .25).X(), 2.5);
    std::ranges::reverse(origins);
    const auto foreign = primitiveEdgeOrigins(document::FeatureId{99}, box);
    origins.insert(origins.begin(), foreign.begin(), foreign.end());
    near(point(origins, topX, .25).X(), 2.5);
    const PrimitiveEdgeOrigin circle{source, Role::cylinderEnd, Role::cylinderSide};
    const auto cylinder = kernel::ShapeFactory::makeCylinderWithHistory({6, 10, 0, 0, 0});
    const auto circles = primitiveEdgeOrigins(source, cylinder);
    require(circles.size() == 2, "Cylinder seam was invented as a projected edge");
    const auto exact = point(circles, circle, .125);
    near(exact.X(), 6 / std::sqrt(2.0));
    near(exact.Y(), 6 / std::sqrt(2.0));
    near(exact.Z(), 10);
    for (unsigned divisions : {3U, 12U, 96U}) {
        const auto display = tessellateEdge(edge(circles, circle), divisions);
        require(display.size() == divisions, "Display segment count");
        const auto segment = std::ranges::find_if(display, [](const auto& s) {
            return s.firstParameter <= .125 && s.secondParameter >= .125;
        });
        require(segment != display.end() && segment->origin == circle,
                "Tessellation lost analytic key");
        const double local =
            (.125 - segment->firstParameter) / (segment->secondParameter - segment->firstParameter);
        const double analytic =
            segment->firstParameter + local * (segment->secondParameter - segment->firstParameter);
        near(point(circles, circle, analytic).Distance(exact), 0);
    }
    near(point(circles, circle, 0).Distance(point(circles, circle, 1)), 0);
    const auto larger = primitiveEdgeOrigins(
        source, kernel::ShapeFactory::makeCylinderWithHistory({12, 10, 0, 0, 0}));
    near(point(larger, circle, .125).X(), 12 / std::sqrt(2.0));
    for (double value : {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try {
            (void)point(circles, circle, value);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Invalid normalized parameter accepted");
    }
    std::cout << "12 semantic box edges, 2 cylinder rings, canonical endpoints, reordered origins "
                 "and density-independent analytic circle points passed\n";
}
TopoDS_Shape tool(double width, double depth, double height, double x, double y, double z) {
    return kernel::ShapeFactory::applyTransform(
        kernel::ShapeFactory::makeBox({width, depth, height}), 0, 0, 0, {x, y, z, {}, {}, {}});
}
void booleanMapping() {
    const auto box = kernel::ShapeFactory::makeBoxWithHistory({10, 10, 10});
    const auto origins = primitiveEdgeOrigins(source, box);
    const auto trimmed = continueEdgeOrigins(
        origins, kernel::ShapeOperations::cutWithHistory(box.shape, tool(5, 12, 12, -1, -1, -1)));
    near(point(trimmed, topX, .75).X(), 7.5);
    require(resolveEdgePoint(trimmed, topX, {EdgePointKind::parameter, .25}).status ==
                EdgeResolutionStatus::parameterOutside,
            "Trim clamped onto remainder");
    require(resolveEdgePoint(trimmed, topX, {EdgePointKind::start}).status ==
                EdgeResolutionStatus::parameterOutside,
            "Deleted original endpoint replaced");
    require(resolveEdgePoint(trimmed, topX, {EdgePointKind::end}).status ==
                EdgeResolutionStatus::resolved,
            "Retained endpoint lost");
    const auto split = continueEdgeOrigins(
        origins, kernel::ShapeOperations::cutWithHistory(box.shape, tool(2, 2, 2, 4, -1, 9)));
    require(resolveEdgePoint(split, topX, {EdgePointKind::parameter, .1}).status ==
                EdgeResolutionStatus::ambiguous,
            "Split selected one piece");
    const auto removed = continueEdgeOrigins(
        origins, kernel::ShapeOperations::cutWithHistory(box.shape, tool(12, 12, 2, -1, -1, 9)));
    require(resolveEdgePoint(removed, topX, {}).status == EdgeResolutionStatus::missing,
            "Deleted edge guessed replacement");
    const auto gap = continueEdgeOrigins(origins, {box.shape, {}, {}});
    require(resolveEdgePoint(gap, topX, {}).status == EdgeResolutionStatus::unavailable,
            "Missing history accepted");
    const auto original = edge(origins, topX);
    const auto equivalent = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 10), gp_Pnt(10, 0, 10)).Edge();
    const kernel::ShapeOperationResult unproved{equivalent,
                                                {},
                                                {{kernel::HistoryOperand::first,
                                                  original.source,
                                                  {equivalent},
                                                  {},
                                                  false,
                                                  false,
                                                  kernel::HistoryCoverage::complete}}};
    const auto unknown = continueEdgeOrigins(std::span{&original, 1}, unproved);
    require(resolveEdgePoint(unknown, topX, {}).status == EdgeResolutionStatus::unavailable,
            "Geometric likeness guessed curve mapping");
    std::cout << "real Boolean trim, original parameter interval, deleted endpoint, split, "
                 "deletion and unproven mapping passed\n";
}
void graphPlacementAndCache() {
    auto model = document::ModelDocument::empty();
    const auto a = model.addBoxFeature("A", {10, 10, 10});
    const auto b = model.addBoxFeature("B", {10, 10, 10});
    FeatureGraph graph;
    const auto first = graph.evaluateDetailed(document::DocumentSnapshot::capture(model));
    require(first->valid(), "Initial graph failed");
    const auto oldEdge =
        edge(output(*first, b).edgeOrigins, {b, Role::boxNegativeY, Role::boxPositiveZ}).source;
    auto params = model.boxParameters(a);
    params.dimensions.width = 20;
    params.placement.rotationY = kernel::Radians{std::numbers::pi / 2};
    require(model.setBoxParameters(a, params), "Edit placement");
    const auto moved = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), first);
    require(moved->valid(), "Placed graph failed");
    const auto p = point(output(*moved, a).edgeOrigins, topX, .25);
    near(p.X(), 15);
    near(p.Y(), 0);
    near(p.Z(), 10);
    require(edge(output(*moved, b).edgeOrigins, {b, Role::boxNegativeY, Role::boxPositiveZ})
                .source.IsSame(oldEdge),
            "Independent edge cache lost");
    require(model.undo(), "Undo placement");
    const auto undo = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), moved);
    near(point(output(*undo, a).edgeOrigins, topX, .25).X(), 2.5);
    require(model.redo(), "Redo placement");
    const auto redo = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), undo);
    near(point(output(*redo, a).edgeOrigins, topX, .25).Distance(p), 0);
    std::cout
        << "recompute, resized/rotated local direction, independent cache and undo/redo passed\n";
}
void graphOperations() {
    auto model = document::ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    const auto cut =
        model.addCylinderFeature("Cut", box, {2, 5, {}, document::CylinderResultMode::cut});
    const auto result = FeatureGraph{}.evaluateDetailed(document::DocumentSnapshot::capture(model));
    require(result->valid(), "Cut graph failed");
    near(point(output(*result, cut).edgeOrigins, topX, .25).X(), 2.5);
    const PrimitiveEdgeOrigin floor{cut, Role::cylinderStart, Role::cylinderSide};
    near(point(output(*result, cut).edgeOrigins, floor, .25).Z(), 8.75);
    require(resolveEdgePoint(output(*result, cut).edgeOrigins,
                             {cut, Role::cylinderEnd, Role::cylinderSide}, {})
                    .status == EdgeResolutionStatus::missing,
            "Tool rim outside result was retained");
    auto extruded = document::ModelDocument::empty();
    const auto base = extruded.addBoxFeature("Base", {10, 10, 10});
    const auto sketch = extruded.addSketchFeature(
        "Sketch", base,
        {{0, 0, 10, 0, 0, 1, 1, 0, 0},
         {{{document::SketchElementId{1}, document::SketchRectangle{2, 2, 4, 4}}}, 2}});
    const auto pad = extruded.addExtrusionFeature("Pad", sketch, {2, {}});
    const auto padded =
        FeatureGraph{}.evaluateDetailed(document::DocumentSnapshot::capture(extruded));
    require(padded->valid(), "Extrusion graph failed");
    near(point(output(*padded, pad).edgeOrigins, topX, .25).X(), 2.5);
    std::cout << "Boolean input/tool edge provenance and extrusion continuation passed\n";
}
} // namespace
int main() {
    try {
        primitivesAndDisplay();
        booleanMapping();
        graphPlacementAndCache();
        graphOperations();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
