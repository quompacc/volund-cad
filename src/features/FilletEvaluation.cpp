// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

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
    // User report 2026-10-25: a face merged from several (a seamless join) carries each of
    // their names; the edge is named by one name per face, the first.
    std::vector<const SupportProvenance*> adjacent;
    std::vector<TopoDS_Shape> faces;
    for (const auto& origin : origins) {
        if (origin.coverage != kernel::HistoryCoverage::complete)
            continue;
        const auto face = std::ranges::find_if(origin.faces, [&](const auto& candidate) {
            EdgeMap edges;
            TopExp::MapShapes(candidate, TopAbs_EDGE, edges);
            return edges.Contains(edge);
        });
        if (face == origin.faces.end() ||
            std::ranges::any_of(faces, [&](const auto& known) { return known.IsSame(*face); }))
            continue;
        faces.push_back(*face);
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
            if (firstEdges.Contains(shared) && std::ranges::none_of(result, [&](const auto& known) {
                    return known.IsSame(shared);
                }))
                result.push_back(shared);
        }
    }
    return result;
}

void numberFilletEdges(std::vector<document::FilletEdge>& edges) {
    std::uint64_t next = 1;
    for (const auto& edge : edges)
        next = std::max(next, edge.id + 1);
    for (auto& edge : edges)
        if (edge.id == 0)
            edge.id = next++;
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

// One rounding (chamfer) step of a body: its feature, edges and value.
struct Step final {
    document::FeatureId feature;
    std::vector<document::FilletEdge> edges;
    double value;
};

std::optional<Step> filletStep(const document::DocumentSnapshot& document, document::FeatureId id,
                               document::BodyId body) {
    const auto& parameters = document.filletParameters(id);
    if (parameters.body != body)
        return std::nullopt;
    return Step{id, parameters.edges, parameters.radius};
}

std::optional<Step> chamferStep(const document::DocumentSnapshot& document, document::FeatureId id,
                                document::BodyId body) {
    const auto& parameters = document.chamferParameters(id);
    if (parameters.body != body)
        return std::nullopt;
    return Step{id, parameters.edges, parameters.distance};
}

using EdgeOperation = kernel::ShapeOperationResult (*)(const TopoDS_Shape&,
                                                       std::span<const TopoDS_Edge>, double);
using EachEdgeOperation = kernel::ShapeOperationResult (*)(const TopoDS_Shape&,
                                                           std::span<const TopoDS_Edge>,
                                                           std::span<const double>);

// M05/M07: what treats the edges and how its failures are named.
struct Treatment final {
    const char* prefix; // code prefix: fillet / chamfer
    const char* verb;   // for the technical message
    kernel::BooleanFailure failure;
    document::FeatureKind kind;
    EdgeOperation apply;
    std::vector<std::size_t> (*failing)(const TopoDS_Shape&, std::span<const TopoDS_Edge>, double);
    EachEdgeOperation applyEach; // earlier steps and this one together
    std::optional<Step> (*step)(const document::DocumentSnapshot&, document::FeatureId,
                                document::BodyId);
};
constexpr Treatment rounding{"fillet",
                             "rounded",
                             kernel::BooleanFailure::filletFailed,
                             document::FeatureKind::fillet,
                             &kernel::ShapeOperations::filletWithHistory,
                             &kernel::ShapeOperations::failingFilletEdges,
                             &kernel::ShapeOperations::filletEachWithHistory,
                             &filletStep};
constexpr Treatment chamfering{"chamfer",
                               "chamfered",
                               kernel::BooleanFailure::chamferFailed,
                               document::FeatureKind::chamfer,
                               &kernel::ShapeOperations::chamferWithHistory,
                               &kernel::ShapeOperations::failingChamferEdges,
                               &kernel::ShapeOperations::chamferEachWithHistory,
                               &chamferStep};

// The body edges where the two named faces meet (a split face may meet in several pieces),
// each with its value and the name of the face it generates (schema 17: {feature, edge id}).
struct ResolvedEdges final {
    std::vector<TopoDS_Edge> edges;
    std::vector<std::uint64_t> owners; // 1-based FilletEdge number of each kernel edge
    std::vector<double> values;
    std::vector<document::SubshapeOrigin> names;
};
void resolveStep(std::span<const SupportProvenance> origins, const Step& step,
                 const Treatment& treatment, ResolvedEdges& result) {
    for (std::size_t index = 0; index < step.edges.size(); ++index) {
        const auto number = static_cast<std::uint64_t>(index + 1);
        const auto shapes = filletEdgeShapes(origins, step.edges[index]);
        if (shapes.empty())
            throw GeometryEvaluationError(
                std::string("The faces of the ") + treatment.verb +
                    " edge are gone or no longer meet" + edgeList({number}),
                std::string(treatment.prefix) + "-edge-missing", {number});
        document::SubshapeOrigin name{step.feature, std::nullopt};
        name.treatedEdge = step.edges[index].id;
        for (const auto& shared : shapes) {
            if (std::ranges::any_of(result.edges,
                                    [&](const auto& known) { return known.IsSame(shared); }))
                continue;
            result.edges.push_back(shared);
            result.owners.push_back(number);
            result.values.push_back(step.value);
            result.names.push_back(name);
        }
    }
}
ResolvedEdges resolveEdges(std::span<const SupportProvenance> origins, std::span<const Step> steps,
                           const Treatment& treatment) {
    ResolvedEdges result;
    for (const auto& step : steps)
        resolveStep(origins, step, treatment, result);
    return result;
}

// User report 2026-10-24: a radius equal to the height of a face beside the edge consumes the
// face, which OCCT cannot build; the message names the largest value the edges at fault take
// (to about 0.01 mm of a 10 mm try, rounded down), so nobody has to guess.
double largestValue(const TopoDS_Shape& body, const ResolvedEdges& resolved,
                    const std::vector<std::size_t>& failing, double value,
                    const Treatment& treatment) {
    std::vector<TopoDS_Edge> edges;
    for (const auto index : failing)
        edges.push_back(resolved.edges[index]);
    double works = 0.0;
    double fails = value;
    for (int step = 0; step < 10; ++step) {
        const double middle = (works + fails) / 2;
        try {
            static_cast<void>(treatment.apply(body, edges, middle));
            works = middle;
        } catch (const kernel::KernelError&) {
            fails = middle;
        }
    }
    return std::floor(works * 100) / 100;
}

kernel::ShapeOperationResult treat(const TopoDS_Shape& body, const ResolvedEdges& resolved,
                                   double value, const Treatment& treatment) {
    try {
        return treatment.apply(body, resolved.edges, value);
    } catch (const kernel::KernelError& error) {
        if (error.failure() != treatment.failure)
            throw;
        const auto indices = treatment.failing(body, resolved.edges, value);
        std::vector<std::uint64_t> failing;
        for (const auto index : indices)
            if (std::ranges::find(failing, resolved.owners[index]) == failing.end())
                failing.push_back(resolved.owners[index]);
        std::ranges::sort(failing);
        const double largest = largestValue(body, resolved, indices, value, treatment);
        std::ostringstream text;
        text << "The edges cannot be " << treatment.verb << " with " << value << edgeList(failing);
        if (largest > 0.0)
            text << "; largest value about " << largest; // FeatureIssueText reads this
        throw GeometryEvaluationError(text.str(), std::string(treatment.prefix) + "-failed",
                                      failing);
    }
}

// The result of a treatment on the shapes of `base` (the body before the treated steps).
struct Treated final {
    document::FeatureId base;
    ResolvedEdges resolved;
    ResolvedEdges referenceResolved;
    kernel::ShapeOperationResult evaluated;
    kernel::ShapeOperationResult reference;
};

// User report 2026-10-24 (as in Fusion): a rounding that runs into an earlier rounding of
// about its radius fails in OCCT one after the other (FreeCAD has the same limit). The earlier
// roundings directly before it on the body are then built again together with it in one step
// on the shape before them, each edge with its own radius, so the corners blend. The earlier
// features keep their own results; only this one's result is the combined shape.
std::optional<Treated> treatWithEarlier(const document::DocumentSnapshot& document, const Step& own,
                                        document::BodyId target, const Treatment& treatment,
                                        const std::vector<EvaluatedFeature>& outputs) {
    constexpr std::size_t deepest = 8;
    std::vector<Step> steps{own};
    auto before = bodyProducerBefore(document, own.feature, target);
    while (before && steps.size() <= deepest) {
        const auto descriptor =
            std::ranges::find(document.features(), *before, &document::FeatureDescriptor::id);
        if (descriptor == document.features().end() || descriptor->kind != treatment.kind)
            return std::nullopt;
        const auto earlier = treatment.step(document, *before, target);
        if (!earlier)
            return std::nullopt;
        steps.push_back(*earlier);
        before = bodyProducerBefore(document, *before, target);
        if (!before)
            return std::nullopt;
        const auto& base = findFeature(outputs, *before);
        try {
            auto resolved = resolveEdges(base.supportOrigins, steps, treatment);
            auto referenceResolved = resolveEdges(base.referenceSupportOrigins, steps, treatment);
            auto evaluated = treatment.applyEach(base.shape, resolved.edges, resolved.values);
            auto reference = treatment.applyEach(base.referenceShape, referenceResolved.edges,
                                                 referenceResolved.values);
            return Treated{*before, std::move(resolved), std::move(referenceResolved),
                           std::move(evaluated), std::move(reference)};
        } catch (const GeometryEvaluationError&) {
        } catch (const kernel::KernelError&) {
        }
    }
    return std::nullopt;
}

// The body's current shape and its reference shape.
using BodyShapes = std::pair<const TopoDS_Shape&, const TopoDS_Shape&>;

Treated treatAlone(const Step& own, document::FeatureId producer, const Treatment& treatment,
                   const std::vector<EvaluatedFeature>& outputs, BodyShapes shapes) {
    const auto& owner = findFeature(outputs, producer);
    auto resolved = resolveEdges(owner.supportOrigins, std::span(&own, 1), treatment);
    auto referenceResolved =
        resolveEdges(owner.referenceSupportOrigins, std::span(&own, 1), treatment);
    auto evaluated = treat(shapes.first, resolved, own.value, treatment);
    auto reference = treat(shapes.second, referenceResolved, own.value, treatment);
    return Treated{producer, std::move(resolved), std::move(referenceResolved),
                   std::move(evaluated), std::move(reference)};
}

// The other faces and edges of the body keep their names; schema 17 (user decision
// 2026-10-13): each face an edge generates is named {feature, edge id}.
std::vector<SupportProvenance> namedFaces(std::vector<SupportProvenance> origins,
                                          const ResolvedEdges& from,
                                          const kernel::ShapeOperationResult& result) {
    for (std::size_t index = 0; index < from.edges.size(); ++index)
        for (const auto& entry : result.history)
            if (entry.source.IsSame(from.edges[index]))
                for (const auto& face : entry.generated)
                    if (face.ShapeType() == TopAbs_FACE)
                        addSupportFace(origins, from.names[index], face);
    return origins;
}

// The treated body as the feature's output; its faces and edges keep their names.
void publish(document::FeatureId feature, const Treated& treated, EvaluatedBody& body,
             EvaluatedBody& referenceBody, std::vector<EvaluatedFeature>& outputs) {
    const auto& base = findFeature(outputs, treated.base);
    EvaluatedFeature output{
        feature,
        treated.evaluated.shape,
        treated.reference.shape,
        body.bodyId,
        namedFaces(continueSupportOrigins(base.supportOrigins, treated.evaluated,
                                          kernel::HistoryOperand::first),
                   treated.resolved, treated.evaluated),
        {},
        namedFaces(continueSupportOrigins(base.referenceSupportOrigins, treated.reference,
                                          kernel::HistoryOperand::first),
                   treated.referenceResolved, treated.reference),
        continueEdgeOrigins(base.edgeOrigins, treated.evaluated, kernel::HistoryOperand::first),
        continueEdgeOrigins(base.referenceEdgeOrigins, treated.reference,
                            kernel::HistoryOperand::first)};
    outputs.push_back(std::move(output));
    body.sourceFeature = feature;
    body.shape = treated.evaluated.shape;
    referenceBody.sourceFeature = feature;
    referenceBody.shape = treated.reference.shape;
}

// N10: the face beside `edge` that `origin` names (the face an unequal chamfer is measured on).
std::optional<TopoDS_Face> faceBeside(std::span<const SupportProvenance> origins,
                                      const document::SubshapeOrigin& origin,
                                      const TopoDS_Edge& edge) {
    const auto* face = named(origins, origin);
    if (face == nullptr)
        return std::nullopt;
    for (const auto& candidate : face->faces) {
        EdgeMap edges;
        TopExp::MapShapes(candidate, TopAbs_EDGE, edges);
        if (edges.Contains(edge))
            return TopoDS::Face(candidate);
    }
    return std::nullopt;
}

std::vector<TopoDS_Face> measuredFaces(std::span<const SupportProvenance> origins,
                                       const document::ChamferParameters& parameters,
                                       const ResolvedEdges& resolved) {
    std::vector<TopoDS_Face> faces;
    for (std::size_t index = 0; index < resolved.edges.size(); ++index) {
        const auto owner = resolved.owners[index];
        const auto& named = parameters.edges[owner - 1];
        const auto face = faceBeside(origins, parameters.shape.flipped ? named.second : named.first,
                                     resolved.edges[index]);
        if (!face)
            throw GeometryEvaluationError("The face a chamfer is measured from is gone" +
                                              edgeList({owner}),
                                          "chamfer-edge-missing", {owner});
        faces.push_back(*face);
    }
    return faces;
}

kernel::ShapeOperationResult treatUnequal(const TopoDS_Shape& body, const ResolvedEdges& resolved,
                                          std::span<const TopoDS_Face> faces,
                                          const document::ChamferParameters& parameters) {
    try {
        return kernel::ShapeOperations::chamferUnequalWithHistory(
            body, resolved.edges, faces,
            {parameters.distance, parameters.shape.second,
             parameters.shape.mode == document::ChamferMode::distanceAngle});
    } catch (const kernel::KernelError& error) {
        if (error.failure() != kernel::BooleanFailure::chamferFailed)
            throw;
        std::vector<std::uint64_t> owners(resolved.owners.begin(), resolved.owners.end());
        owners.erase(std::ranges::unique(owners).begin(), owners.end());
        throw GeometryEvaluationError("The edges cannot be chamfered with these values" +
                                          edgeList(owners),
                                      "chamfer-failed", owners);
    }
}

// N10: a chamfer with two distances or a distance and an angle, on the body before it.
void evaluateUnequalChamfer(const document::DocumentSnapshot& document,
                            const document::FeatureDescriptor& descriptor,
                            std::vector<EvaluatedFeature>& outputs,
                            std::vector<EvaluatedBody>& bodies,
                            std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.chamferParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body has no solid before this feature", "body-missing");
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto& owner = findFeature(outputs, *producer);
    const Step own{descriptor.id, parameters.edges, parameters.distance};
    auto resolved = resolveEdges(owner.supportOrigins, std::span(&own, 1), chamfering);
    auto referenceResolved =
        resolveEdges(owner.referenceSupportOrigins, std::span(&own, 1), chamfering);
    auto evaluated =
        treatUnequal(body.shape, resolved,
                     measuredFaces(owner.supportOrigins, parameters, resolved), parameters);
    auto reference = treatUnequal(
        referenceBody.shape, referenceResolved,
        measuredFaces(owner.referenceSupportOrigins, parameters, referenceResolved), parameters);
    publish(descriptor.id,
            Treated{*producer, std::move(resolved), std::move(referenceResolved),
                    std::move(evaluated), std::move(reference)},
            body, referenceBody, outputs);
}

void evaluateEdgeTreatment(const document::DocumentSnapshot& document,
                           const document::FeatureDescriptor& descriptor, document::BodyId target,
                           const std::vector<document::FilletEdge>& edges, double value,
                           const Treatment& treatment, std::vector<EvaluatedFeature>& outputs,
                           std::vector<EvaluatedBody>& bodies,
                           std::vector<EvaluatedBody>& referenceBodies) {
    const auto producer = bodyProducerBefore(document, descriptor.id, target);
    if (!producer)
        throw GeometryEvaluationError("The body has no solid before this feature", "body-missing");
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const Step own{descriptor.id, edges, value};
    std::optional<Treated> treated;
    try {
        treated = treatAlone(own, *producer, treatment, outputs,
                             BodyShapes{body.shape, referenceBody.shape});
    } catch (const GeometryEvaluationError& error) {
        if (error.code() != std::string(treatment.prefix) + "-failed")
            throw;
        treated = treatWithEarlier(document, own, target, treatment, outputs);
        if (!treated)
            throw;
    }
    publish(descriptor.id, *treated, body, referenceBody, outputs);
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
    if (parameters.shape.mode != document::ChamferMode::equal) // N10
        return evaluateUnequalChamfer(document, descriptor, outputs, bodies, referenceBodies);
    evaluateEdgeTreatment(document, descriptor, parameters.body, parameters.edges,
                          parameters.distance, chamfering, outputs, bodies, referenceBodies);
}

} // namespace voelund::features::detail
