#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <gp_Vec.hxx>
#include <utility>
#include <vector>

namespace voelund::features::detail {
namespace {
struct Prism final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> caps;
    std::vector<EdgeProvenance> edges;
};
// The selected regions swept along the sketch normal, against it when reversed.
Prism makePrism(document::FeatureId extrusion, document::FeatureId sketchId,
                const document::SketchParameters& sketch,
                const document::ExtrusionParameters& parameters) {
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    for (const auto& profile : profiles)
        builder.Add(faces, profile.face);
    // Exact support-plane contact: preserve both the start plane and the requested
    // end plane. OCCT's Boolean classifies the coincident faces; no length padding.
    const double length = parameters.reversed ? -parameters.distance : parameters.distance;
    BRepPrimAPI_MakePrism prism(
        faces, gp_Vec(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ) * length);
    Prism result{prism.Shape(), {}, extrusionEdgeOrigins(extrusion, sketchId, profiles, prism)};
    for (const auto& profile : profiles) {
        const auto cap = prism.LastShape(profile.face);
        result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                               {cap},
                               kernel::HistoryCoverage::complete,
                               cap});
    }
    return result;
}

kernel::ShapeOperationResult combine(document::ExtrusionOperation operation,
                                     const TopoDS_Shape& body, const TopoDS_Shape& tool) {
    return operation == document::ExtrusionOperation::cut
               ? kernel::ShapeOperations::cutWithHistory(body, tool)
               : kernel::ShapeOperations::fuseWithHistory(body, tool);
}

template <class Origins>
Origins continued(const Origins& input, const Origins& tool,
                  const kernel::ShapeOperationResult& result, auto continueOrigins) {
    auto origins = continueOrigins(input, result, kernel::HistoryOperand::first);
    auto added = continueOrigins(tool, result, kernel::HistoryOperand::second);
    origins.insert(origins.end(), added.begin(), added.end());
    return origins;
}
} // namespace

void evaluateExtrusion(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    if (!descriptor.inputFeature.has_value()) {
        throw GeometryEvaluationError("Extrusion requires a sketch input");
    }
    const document::FeatureId sketchId = *descriptor.inputFeature;
    const auto& projected = findFeature(outputs, sketchId).resolvedSketch;
    if (!projected)
        throw GeometryEvaluationError("Current sketch projection is missing");
    const document::SketchParameters& sketch = *projected;
    const document::ExtrusionParameters& parameters = document.extrusionParameters(descriptor.id);
    const auto descriptors = document.features();
    const auto sketchDescriptor =
        std::ranges::find(descriptors, sketchId, &document::FeatureDescriptor::id);
    if (sketchDescriptor == descriptors.end())
        throw GeometryEvaluationError("Extrusion sketch is missing");
    const auto support = sketchDescriptor->inputFeature;
    const auto operation = document::effectiveOperation(parameters, !support);
    const auto prism = makePrism(descriptor.id, sketchId, sketch, parameters);

    // The reference evaluation resolves a bound face sketch on the support's reference
    // result; a base-plane sketch is its own reference.
    auto referenceSketch = sketch;
    if (support && sketch.supportBinding) {
        const auto& supportOutput = findFeature(outputs, *support);
        const EvaluatedFeature referenceSupport{supportOutput.id,
                                                supportOutput.referenceShape,
                                                supportOutput.referenceShape,
                                                supportOutput.bodyId,
                                                supportOutput.referenceSupportOrigins,
                                                {},
                                                {},
                                                supportOutput.referenceEdgeOrigins};
        resolveBoundSketchSupport(referenceSketch, referenceSupport);
        if (solveSketchConstraints(referenceSketch).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Reference sketch projected edges cannot be solved");
    }
    const auto referencePrism = makePrism(descriptor.id, sketchId, referenceSketch, parameters);

    if (operation == document::ExtrusionOperation::newBody) {
        // V01: a base-plane sketch opened its own body (BodyId = sketch FeatureId); T02: a
        // new body on a face is the extrusion's own. Nothing is fused and nothing is guessed.
        const document::BodyId bodyId{support ? descriptor.id.value() : sketchId.value()};
        if (std::ranges::find(bodies, bodyId, &EvaluatedBody::bodyId) != bodies.end())
            throw GeometryEvaluationError("Base-plane body already has a solid");
        outputs.push_back(EvaluatedFeature{descriptor.id, prism.shape, referencePrism.shape,
                                           bodyId, prism.caps, {}, referencePrism.caps,
                                           prism.edges, referencePrism.edges});
        bodies.push_back({bodyId, descriptor.id, prism.shape});
        referenceBodies.push_back({bodyId, descriptor.id, referencePrism.shape});
        return;
    }

    // Join or cut works on the support's body, or on the body a base-plane sketch names.
    std::optional<document::FeatureId> input = support;
    if (!input) {
        if (!parameters.target)
            throw GeometryEvaluationError("Join or cut of a base-plane sketch needs a target body");
        input = targetProducer(document, descriptor.id);
        if (!input)
            throw GeometryEvaluationError("Target body has no solid before this extrusion");
    }
    EvaluatedBody& body = findBody(bodies, outputs, *input);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *input);
    const auto evaluated = combine(operation, body.shape, prism.shape);
    const auto reference = combine(operation, referenceBody.shape, referencePrism.shape);
    const auto& inputOutput = findFeature(outputs, *input);
    const auto supportOrigins = [](const auto& previous, const auto& result, auto operand) {
        return continueSupportOrigins(previous, result, operand);
    };
    const auto edgeOrigins = [](const auto& previous, const auto& result, auto operand) {
        return continueEdgeOrigins(previous, result, operand);
    };
    auto origins = continued(inputOutput.supportOrigins, prism.caps, evaluated, supportOrigins);
    auto referenceOrigins = continued(inputOutput.referenceSupportOrigins, referencePrism.caps,
                                      reference, supportOrigins);
    auto edges = continued(inputOutput.edgeOrigins, prism.edges, evaluated, edgeOrigins);
    auto referenceEdges = continued(inputOutput.referenceEdgeOrigins, referencePrism.edges,
                                    reference, edgeOrigins);
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       evaluated.shape,
                                       reference.shape,
                                       body.bodyId,
                                       std::move(origins),
                                       {},
                                       std::move(referenceOrigins),
                                       std::move(edges),
                                       std::move(referenceEdges)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
