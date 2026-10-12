#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <string>

// M05 (user decisions 2026-10-12): a fillet rounds edges of one body with one radius. Each
// edge is named by its two adjacent faces; their names follow the faces through the body's
// history, and the edge is where the faces meet now. A missing face or a radius the kernel
// cannot build fails the feature with the edges at fault; the body keeps its earlier shape.
namespace voelund::features::detail {
namespace {

using EdgeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

std::string edgeList(const std::vector<std::uint64_t>& edges) {
    std::string text = " [edges";
    for (const auto edge : edges)
        text += " " + std::to_string(edge);
    return text + "]";
}

const std::vector<TopoDS_Shape>& faceOf(std::span<const SupportProvenance> origins,
                                        const document::SubshapeOrigin& origin,
                                        std::uint64_t edgeNumber) {
    const auto found = std::ranges::find(origins, origin, &SupportProvenance::origin);
    if (found == origins.end() || found->faces.empty() ||
        found->coverage != kernel::HistoryCoverage::complete)
        throw GeometryEvaluationError("A face of the rounded edge no longer exists" +
                                          edgeList({edgeNumber}),
                                      "fillet-edge-missing", {edgeNumber});
    return found->faces;
}

// The body edges where the two named faces meet (a split face may meet in several pieces).
struct ResolvedEdges final {
    std::vector<TopoDS_Edge> edges;
    std::vector<std::uint64_t> owners; // 1-based FilletEdge number of each kernel edge
};
ResolvedEdges resolveEdges(std::span<const SupportProvenance> origins,
                           const document::FilletParameters& parameters) {
    ResolvedEdges result;
    for (std::size_t index = 0; index < parameters.edges.size(); ++index) {
        const auto number = static_cast<std::uint64_t>(index + 1);
        const auto& edge = parameters.edges[index];
        EdgeMap first;
        for (const auto& face : faceOf(origins, edge.first, number))
            TopExp::MapShapes(face, TopAbs_EDGE, first);
        std::size_t found = 0;
        for (const auto& face : faceOf(origins, edge.second, number)) {
            EdgeMap second;
            TopExp::MapShapes(face, TopAbs_EDGE, second);
            for (int item = 1; item <= second.Extent(); ++item) {
                if (!first.Contains(second(item)))
                    continue;
                const TopoDS_Edge shared = TopoDS::Edge(second(item));
                if (std::ranges::any_of(result.edges,
                                        [&](const auto& known) { return known.IsSame(shared); }))
                    continue;
                result.edges.push_back(shared);
                result.owners.push_back(number);
                ++found;
            }
        }
        if (found == 0)
            throw GeometryEvaluationError("The faces of the rounded edge no longer meet" +
                                              edgeList({number}),
                                          "fillet-edge-missing", {number});
    }
    return result;
}

kernel::ShapeOperationResult round(const TopoDS_Shape& body, const ResolvedEdges& resolved,
                                   double radius) {
    try {
        return kernel::ShapeOperations::filletWithHistory(body, resolved.edges, radius);
    } catch (const kernel::KernelError& error) {
        if (error.failure() != kernel::BooleanFailure::filletFailed)
            throw;
        std::vector<std::uint64_t> failing;
        for (const auto index :
             kernel::ShapeOperations::failingFilletEdges(body, resolved.edges, radius))
            if (std::ranges::find(failing, resolved.owners[index]) == failing.end())
                failing.push_back(resolved.owners[index]);
        std::ranges::sort(failing);
        throw GeometryEvaluationError(std::string("The edges cannot be rounded with radius ") +
                                          std::to_string(radius) + edgeList(failing),
                                      "fillet-failed", failing);
    }
}

} // namespace

void evaluateFillet(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.filletParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The fillet's body has no solid before it");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto evaluated =
        round(body.shape, resolveEdges(owner.supportOrigins, parameters), parameters.radius);
    const auto reference = round(referenceBody.shape,
                                 resolveEdges(owner.referenceSupportOrigins, parameters),
                                 parameters.radius);
    // Fillet faces are generated and stay unnamed in M05; the body's other faces and edges
    // keep their names.
    outputs.push_back(EvaluatedFeature{
        descriptor.id, evaluated.shape, reference.shape, body.bodyId,
        continueSupportOrigins(owner.supportOrigins, evaluated, kernel::HistoryOperand::first),
        {},
        continueSupportOrigins(owner.referenceSupportOrigins, reference,
                               kernel::HistoryOperand::first),
        continueEdgeOrigins(owner.edgeOrigins, evaluated, kernel::HistoryOperand::first),
        continueEdgeOrigins(owner.referenceEdgeOrigins, reference, kernel::HistoryOperand::first)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
