#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

namespace voelund::features {
namespace {
bool contains(const TopoDS_Shape& shape, const TopoDS_Edge& edge) {
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next())
        if (it.Current().IsSame(edge))
            return true;
    return false;
}
EdgeProvenance provenance(ContourEdgeOrigin origin, const EvaluatedContourEdge& source,
                          const TopoDS_Shape& mapped, const TopoDS_Shape& owner) {
    if (mapped.IsNull() || mapped.ShapeType() != TopAbs_EDGE)
        throw GeometryEvaluationError("Contour builder did not supply an edge");
    const auto edge = TopoDS::Edge(mapped.Oriented(TopAbs_FORWARD));
    if (!contains(owner, edge))
        throw GeometryEvaluationError("Named contour is absent from its result");
    const BRepAdaptor_Curve curve(edge);
    // A prism translates the original contour without reparameterization.
    // Seed the current source from that explicit generation result, not from a
    // geometrically similar edge or an explorer position.
    if (curve.FirstParameter() != source.startParameter ||
        curve.LastParameter() != source.endParameter)
        throw GeometryEvaluationError("Prism changed the contour parameter domain");
    if (!source.domain)
        return {origin, edge, source.startParameter, source.endParameter, {edge}};
    // R10b: a fragment refers to its whole element. The source spans the full oriented
    // domain on the same curve, so continued history keeps proving the parameter mapping.
    TopLoc_Location location;
    double first = 0, last = 0;
    const auto shared = BRep_Tool::Curve(edge, location, first, last);
    TopoDS_Edge whole = BRepBuilderAPI_MakeEdge(shared, source.domain->first, source.domain->second);
    whole.Location(location);
    return {origin, whole, source.domain->first, source.domain->second, {edge}};
}
} // namespace
std::vector<EdgeProvenance> sketchEdgeOrigins(document::FeatureId sketch,
                                              std::span<const EvaluatedSketchElement> elements) {
    std::vector<EdgeProvenance> result;
    for (const auto& element : elements) {
        if (element.construction)
            continue;
        for (const auto& edge : element.edges) {
            // An individual curve is also its own owner; explorer membership
            // includes an edge root in OCCT.
            result.push_back(
                provenance({sketch, edge.element, edge.role}, edge, edge.edge, element.shape));
        }
    }
    return result;
}
std::vector<EdgeProvenance> extrusionEdgeOrigins(document::FeatureId extrusion,
                                                 document::FeatureId sketch,
                                                 std::span<const EvaluatedProfileFace> profiles,
                                                 BRepPrimAPI_MakePrism& prism) {
    std::vector<EdgeProvenance> result;
    for (const auto& profile : profiles)
        for (const auto& edge : profile.edges) {
            result.push_back(provenance(
                {sketch, edge.element, edge.role, extrusion, ContourStage::extrusionStart}, edge,
                prism.FirstShape(edge.edge), prism.Shape()));
            result.push_back(
                provenance({sketch, edge.element, edge.role, extrusion, ContourStage::extrusionEnd},
                           edge, prism.LastShape(edge.edge), prism.Shape()));
        }
    return result;
}
} // namespace voelund::features
