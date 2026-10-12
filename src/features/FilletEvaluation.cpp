#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>
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
namespace voelund::features {
namespace {
using EdgeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
const SupportProvenance* named(std::span<const SupportProvenance> origins,
                               const document::SubshapeOrigin& origin) {
    const auto found = std::ranges::find(origins, origin, &SupportProvenance::origin);
    return found == origins.end() || found->faces.empty() ||
                   found->coverage != kernel::HistoryCoverage::complete
               ? nullptr
               : &*found;
}
} // namespace

std::optional<document::FilletEdge> filletEdgeAt(std::span<const SupportProvenance> origins,
                                                 const TopoDS_Shape& edge) {
    std::vector<const SupportProvenance*> adjacent;
    for (const auto& origin : origins) {
        if (origin.coverage != kernel::HistoryCoverage::complete)
            continue;
        const bool contains = std::ranges::any_of(origin.faces, [&](const auto& face) {
            EdgeMap edges;
            TopExp::MapShapes(face, TopAbs_EDGE, edges);
            return edges.Contains(edge);
        });
        if (contains)
            adjacent.push_back(&origin);
    }
    if (adjacent.size() != 2 || adjacent[0]->origin == adjacent[1]->origin)
        return std::nullopt;
    return document::FilletEdge{adjacent[0]->origin, adjacent[1]->origin};
}

std::vector<TopoDS_Edge> filletEdgeShapes(std::span<const SupportProvenance> origins,
                                          const document::FilletEdge& edge) {
    std::vector<TopoDS_Edge> result;
    const auto* first = named(origins, edge.first);
    const auto* second = named(origins, edge.second);
    if (first == nullptr || second == nullptr)
        return result;
    EdgeMap firstEdges;
    for (const auto& face : first->faces)
        TopExp::MapShapes(face, TopAbs_EDGE, firstEdges);
    for (const auto& face : second->faces) {
        EdgeMap secondEdges;
        TopExp::MapShapes(face, TopAbs_EDGE, secondEdges);
        for (int item = 1; item <= secondEdges.Extent(); ++item) {
            const TopoDS_Edge shared = TopoDS::Edge(secondEdges(item));
            if (firstEdges.Contains(shared) &&
                std::ranges::none_of(result, [&](const auto& known) { return known.IsSame(shared); }))
                result.push_back(shared);
        }
    }
    return result;
}

bool sameFilletEdge(const document::FilletEdge& a, const document::FilletEdge& b) noexcept {
    return (a.first == b.first && a.second == b.second) ||
           (a.first == b.second && a.second == b.first);
}
} // namespace voelund::features

namespace voelund::features::detail {
namespace {

std::string edgeList(const std::vector<std::uint64_t>& edges) {
    std::string text = " [edges";
    for (const auto edge : edges)
        text += " " + std::to_string(edge);
    return text + "]";
}

// M05/M07: what treats the edges and how its failures are named.
struct Treatment final {
    const char* prefix; // code prefix: fillet / chamfer
    const char* verb;   // for the technical message
    kernel::BooleanFailure failure;
    kernel::ShapeOperationResult (*apply)(const TopoDS_Shape&, std::span<const TopoDS_Edge>,
                                          double);
    std::vector<std::size_t> (*failing)(const TopoDS_Shape&, std::span<const TopoDS_Edge>,
                                        double);
};
constexpr Treatment rounding{"fillet", "rounded", kernel::BooleanFailure::filletFailed,
                             &kernel::ShapeOperations::filletWithHistory,
                             &kernel::ShapeOperations::failingFilletEdges};
constexpr Treatment chamfering{"chamfer", "chamfered", kernel::BooleanFailure::chamferFailed,
                               &kernel::ShapeOperations::chamferWithHistory,
                               &kernel::ShapeOperations::failingChamferEdges};

// The body edges where the two named faces meet (a split face may meet in several pieces).
struct ResolvedEdges final {
    std::vector<TopoDS_Edge> edges;
    std::vector<std::uint64_t> owners; // 1-based FilletEdge number of each kernel edge
};
ResolvedEdges resolveEdges(std::span<const SupportProvenance> origins,
                           const std::vector<document::FilletEdge>& edges,
                           const Treatment& treatment) {
    ResolvedEdges result;
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const auto number = static_cast<std::uint64_t>(index + 1);
        const auto shapes = filletEdgeShapes(origins, edges[index]);
        if (shapes.empty())
            throw GeometryEvaluationError(std::string("The faces of the ") + treatment.verb +
                                              " edge are gone or no longer meet" +
                                              edgeList({number}),
                                          std::string(treatment.prefix) + "-edge-missing",
                                          {number});
        for (const auto& shared : shapes) {
            if (std::ranges::any_of(result.edges,
                                    [&](const auto& known) { return known.IsSame(shared); }))
                continue;
            result.edges.push_back(shared);
            result.owners.push_back(number);
        }
    }
    return result;
}

kernel::ShapeOperationResult treat(const TopoDS_Shape& body, const ResolvedEdges& resolved,
                                   double value, const Treatment& treatment) {
    try {
        return treatment.apply(body, resolved.edges, value);
    } catch (const kernel::KernelError& error) {
        if (error.failure() != treatment.failure)
            throw;
        std::vector<std::uint64_t> failing;
        for (const auto index : treatment.failing(body, resolved.edges, value))
            if (std::ranges::find(failing, resolved.owners[index]) == failing.end())
                failing.push_back(resolved.owners[index]);
        std::ranges::sort(failing);
        throw GeometryEvaluationError(std::string("The edges cannot be ") + treatment.verb +
                                          " with " + std::to_string(value) + edgeList(failing),
                                      std::string(treatment.prefix) + "-failed", failing);
    }
}

void evaluateEdgeTreatment(const document::DocumentSnapshot& document,
                           const document::FeatureDescriptor& descriptor, document::BodyId target,
                           const std::vector<document::FilletEdge>& edges, double value,
                           const Treatment& treatment, std::vector<EvaluatedFeature>& outputs,
                           std::vector<EvaluatedBody>& bodies,
                           std::vector<EvaluatedBody>& referenceBodies) {
    const auto producer = bodyProducerBefore(document, descriptor.id, target);
    if (!producer)
        throw GeometryEvaluationError("The body has no solid before this feature");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto evaluated =
        treat(body.shape, resolveEdges(owner.supportOrigins, edges, treatment), value, treatment);
    const auto reference =
        treat(referenceBody.shape, resolveEdges(owner.referenceSupportOrigins, edges, treatment),
              value, treatment);
    // Fillet and chamfer faces are generated and stay unnamed; the other faces and edges of
    // the body keep their names.
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

} // namespace

void evaluateFillet(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.filletParameters(descriptor.id);
    evaluateEdgeTreatment(document, descriptor, parameters.body, parameters.edges,
                          parameters.radius, rounding, outputs, bodies, referenceBodies);
}

void evaluateChamfer(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.chamferParameters(descriptor.id);
    evaluateEdgeTreatment(document, descriptor, parameters.body, parameters.edges,
                          parameters.distance, chamfering, outputs, bodies, referenceBodies);
}

} // namespace voelund::features::detail
