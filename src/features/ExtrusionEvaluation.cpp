#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pnt.hxx>
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
// The sweep direction: the sketch normal, reversed on request.
gp_Vec direction(const document::SketchParameters& sketch,
                 const document::ExtrusionParameters& parameters) {
    const gp_Vec normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    return parameters.reversed ? -normal : normal;
}
// T03: the sketch moved by the start offset along the sweep direction.
document::SketchParameters startSketch(document::SketchParameters sketch,
                                       const document::ExtrusionParameters& parameters) {
    const auto shift = direction(sketch, parameters) * parameters.offset;
    sketch.plane.originX += shift.X();
    sketch.plane.originY += shift.Y();
    sketch.plane.originZ += shift.Z();
    return sketch;
}
// T03: the length from the start plane to a parallel end face ahead of it.
double lengthToFace(const document::SketchParameters& start,
                    const document::ExtrusionParameters& parameters,
                    const EvaluatedFeature& faceOwner, const document::SubshapeOrigin& origin) {
    const auto faces = supportFaceFrames(faceOwner);
    const auto face = std::ranges::find(faces, origin, &SupportFaceFrame::origin);
    if (face == faces.end())
        throw GeometryEvaluationError("End face is no longer a unique planar face of its body");
    const auto along = direction(start, parameters);
    const gp_Vec normal(face->plane.normalX, face->plane.normalY, face->plane.normalZ);
    if (along.CrossMagnitude(normal) > std::sin(kernel::angularTolerance.radians))
        throw GeometryEvaluationError("End face must be parallel to the sketch");
    const gp_Vec offset(gp_Pnt(start.plane.originX, start.plane.originY, start.plane.originZ),
                        gp_Pnt(face->plane.originX, face->plane.originY, face->plane.originZ));
    const double length = offset.Dot(along);
    if (length <= 1e-6)
        throw GeometryEvaluationError("End face does not lie ahead in the extrusion direction");
    return length;
}
// The selected regions swept along the sketch normal, against it when reversed.
Prism makePrism(document::FeatureId extrusion, document::FeatureId sketchId,
                const document::SketchParameters& sketch,
                const document::ExtrusionParameters& parameters, double length) {
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    for (const auto& profile : profiles)
        builder.Add(faces, profile.face);
    // Exact support-plane contact: preserve both the start plane and the requested
    // end plane. OCCT's Boolean classifies the coincident faces; no length padding.
    BRepPrimAPI_MakePrism prism(faces, direction(sketch, parameters) * length);
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
    if (const auto& own = document.extrusionParameters(descriptor.id); own.faceProfile) {
        evaluateFaceExtrusion(document, descriptor, own, outputs, bodies, referenceBodies);
        return;
    }
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
    // T03: the end face's body as it was before this extrusion, current and reference.
    std::optional<document::FeatureId> faceOwner;
    if (parameters.upToFace) {
        faceOwner = bodyProducerBefore(document, descriptor.id, parameters.upToFace->body);
        if (!faceOwner)
            throw GeometryEvaluationError("End face body has no solid before this extrusion");
    }
    const auto start = startSketch(sketch, parameters);
    // The reference prism keeps this length: reference results are unplaced primitives, so
    // the end face's reference position says nothing about the current extent.
    const double length =
        faceOwner ? lengthToFace(start, parameters, findFeature(outputs, *faceOwner),
                                 parameters.upToFace->origin)
                  : parameters.distance;
    const auto prism = makePrism(descriptor.id, sketchId, start, parameters, length);

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
    const auto referenceStart = startSketch(referenceSketch, parameters);
    const auto referencePrism =
        makePrism(descriptor.id, sketchId, referenceStart, parameters, length);

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
