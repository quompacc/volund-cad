#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pln.hxx>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

using namespace voelund;
namespace {
using features::SupportResolutionStatus;
using kernel::PrimitiveRole;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double value, double expected) {
    require(std::abs(value - expected) < 1e-7, "Geometry mismatch");
}
const features::EvaluatedFeature& output(const features::FeatureGraphEvaluation& evaluation,
                                         document::FeatureId id) {
    const auto found =
        std::ranges::find(evaluation.result.features, id, &features::EvaluatedFeature::id);
    require(found != evaluation.result.features.end(), "Feature output missing");
    return *found;
}
features::SupportResolution resolve(const features::FeatureGraphEvaluation& evaluation,
                                    document::FeatureId target, document::FeatureId origin,
                                    PrimitiveRole role) {
    return features::resolveSupportOrigin(output(evaluation, target).supportOrigins,
                                          {origin, role});
}
TopoDS_Shape unique(const features::SupportResolution& result) {
    require(result.status == SupportResolutionStatus::resolved && result.candidates.size() == 1,
            "Expected one proven support face");
    return result.candidates.front();
}
void placementAndCache() {
    auto model = document::ModelDocument::empty();
    const auto a = model.addBoxFeature("A", {10, 10, 10});
    const auto b = model.addBoxFeature("B", {10, 10, 10});
    auto bp = model.boxParameters(b);
    bp.placement.translationX = 20;
    require(model.setBoxParameters(b, bp), "Place B");
    features::FeatureGraph graph;
    const auto first = graph.evaluateDetailed(document::DocumentSnapshot::capture(model));
    const auto faceA = unique(resolve(*first, a, a, PrimitiveRole::boxPositiveZ));
    const auto faceB = unique(resolve(*first, b, b, PrimitiveRole::boxPositiveZ));
    require(!faceA.IsSame(faceB), "Coplanar bodies share an identity");
    require(resolve(*first, a, b, PrimitiveRole::boxPositiveZ).status ==
                SupportResolutionStatus::missing,
            "Foreign origin accepted in body A");
    auto ap = model.boxParameters(a);
    ap.dimensions.height = 12;
    ap.placement.rotationY = kernel::Radians{std::numbers::pi / 2};
    require(model.setBoxParameters(a, ap), "Rotate/resize A");
    const auto edited = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), first);
    const auto moved = unique(resolve(*edited, a, a, PrimitiveRole::boxPositiveZ));
    const BRepAdaptor_Surface surface(TopoDS::Face(moved));
    near(std::abs(surface.Plane().Axis().Direction().X()), 1);
    near(surface.Plane().Location().X(), 11); // pivot x=5, half height=6
    const BRepAdaptor_Surface previousSurface(TopoDS::Face(faceA));
    near(previousSurface.Plane().Location().Z(), 10);
    near(std::abs(previousSurface.Plane().Axis().Direction().Z()), 1);
    require(unique(resolve(*edited, b, b, PrimitiveRole::boxPositiveZ)).IsSame(faceB),
            "Independent cached support provenance changed");
    const auto repeated =
        graph.evaluateDetailed(document::DocumentSnapshot::capture(model), edited);
    require(unique(resolve(*repeated, a, a, PrimitiveRole::boxPositiveZ)).IsSame(moved),
            "Cache dropped support lineage");
    std::cout << "coplanar identity, resized/rotated source and independent cache passed\n";
}
void booleanAndExtrusion() {
    auto model = document::ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    const auto cut = model.addCylinderFeature(
        "Cut", box, {2, 5, {0, 0, 0, {}, {}, {}}, document::CylinderResultMode::cut});
    features::FeatureGraph graph;
    const auto first = graph.evaluateDetailed(document::DocumentSnapshot::capture(model));
    require(first->valid(), "Cut evaluation failed");
    GProp_GProps area;
    BRepGProp::SurfaceProperties(unique(resolve(*first, cut, box, PrimitiveRole::boxPositiveZ)),
                                 area);
    near(area.Mass(), 100 - 4 * std::numbers::pi);
    auto parameters = model.cylinderParameters(cut);
    parameters.radius = 20;
    require(model.setCylinderParameters(cut, parameters), "Enlarge cut");
    const auto removed = graph.evaluateDetailed(document::DocumentSnapshot::capture(model), first);
    require(removed->valid(), "Slab removal invalid");
    require(resolve(*removed, cut, box, PrimitiveRole::boxPositiveZ).status ==
                SupportResolutionStatus::missing,
            "Deleted top silently replaced by new parallel cut surface");
    require(model.undo(), "Undo deletion");
    const auto restored =
        graph.evaluateDetailed(document::DocumentSnapshot::capture(model), removed);
    (void)unique(resolve(*restored, cut, box, PrimitiveRole::boxPositiveZ));
    require(model.redo(), "Redo deletion");
    const auto redone =
        graph.evaluateDetailed(document::DocumentSnapshot::capture(model), restored);
    require(resolve(*redone, cut, box, PrimitiveRole::boxPositiveZ).status ==
                SupportResolutionStatus::missing,
            "Redo lost deleted provenance");
    auto extruded = document::ModelDocument::empty();
    const auto base = extruded.addBoxFeature("Base", {10, 10, 10});
    const auto sketch = extruded.addSketchFeature(
        "Sketch", base,
        {{0, 0, 10, 0, 0, 1, 1, 0, 0},
         {{{document::SketchElementId{1}, document::SketchRectangle{1, 1, 3, 3}}}, 2}});
    const auto extrusion = extruded.addExtrusionFeature("Extrusion", sketch, {2, {}});
    const auto result = graph.evaluateDetailed(document::DocumentSnapshot::capture(extruded));
    require(result->valid(), "Extrusion evaluation failed");
    (void)unique(resolve(*result, extrusion, base, PrimitiveRole::boxNegativeZ));
    require(resolve(*result, extrusion, extrusion, PrimitiveRole::boxPositiveZ).status ==
                SupportResolutionStatus::missing,
            "Unnamed extrusion face given a fabricated primitive role");
    std::cout << "Boolean ancestry, deletion, undo/redo and extrusion input lineage passed\n";
}
void splitAndHistoryGap() {
    const auto primitive = kernel::ShapeFactory::makeBoxWithHistory({10, 10, 10});
    const document::FeatureId source{1};
    auto origins = features::primitiveSupportOrigins(source, primitive);
    const auto tool = kernel::ShapeFactory::applyTransform(
        kernel::ShapeFactory::makeBox({2, 12, 2}), 0, 0, 0, {4, -1, 9, {}, {}, {}});
    const auto cut = kernel::ShapeOperations::cutWithHistory(primitive.shape, tool);
    auto split = features::continueSupportOrigins(origins, cut);
    require(features::resolveSupportOrigin(split, {source, PrimitiveRole::boxPositiveZ}).status ==
                SupportResolutionStatus::ambiguous,
            "Split auto-selected a face");
    const kernel::ShapeOperationResult gap{cut.shape, {}, {}};
    auto missing = features::continueSupportOrigins(split, gap);
    require(features::resolveSupportOrigin(missing, {source, PrimitiveRole::boxPositiveZ}).status ==
                SupportResolutionStatus::unavailable,
            "Missing history guessed a face");
    // A later empty projection cannot turn an unknown ancestry into proven deletion.
    missing = features::continueSupportOrigins(missing, gap);
    require(features::resolveSupportOrigin(missing, {source, PrimitiveRole::boxPositiveZ}).status ==
                SupportResolutionStatus::unavailable,
            "History gap became a complete tombstone");
    std::cout << "split ambiguity and persistent history gap passed\n";
}
} // namespace
int main() {
    try {
        placementAndCache();
        booleanAndExtrusion();
        splitAndHistoryGap();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
