#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <algorithm>

// M25 (user decision 2026-10-14): tilts named faces of a body about the plane of the neutral
// face, pulled into the body (against the face's outward normal): a positive angle tapers the
// body away from the neutral face, as a mould needs. The faces are found by their names on the body before the
// draft; they keep their names. A face the kernel cannot tilt is reported by its number.
namespace voelund::features::detail {
namespace {

std::vector<TopoDS_Shape> draftFaces(std::span<const SupportProvenance> origins,
                                     const document::DraftParameters& parameters) {
    std::vector<TopoDS_Shape> faces;
    for (std::size_t index = 0; index < parameters.faces.size(); ++index) {
        const auto found =
            std::ranges::find(origins, parameters.faces[index], &SupportProvenance::origin);
        if (found == origins.end() || found->faces.size() != 1)
            throw GeometryEvaluationError("A face the draft tilts is no longer on its body",
                                          "draft-face-missing", {index + 1});
        faces.push_back(found->faces.front());
    }
    return faces;
}

kernel::ShapeOperationResult tilt(const TopoDS_Shape& body, const EvaluatedFeature& owner,
                                  std::span<const SupportProvenance> origins,
                                  const document::DraftParameters& parameters) {
    const auto frames = supportFaceFrames(owner);
    const auto neutral = std::ranges::find(frames, parameters.neutral, &SupportFaceFrame::origin);
    if (neutral == frames.end())
        throw GeometryEvaluationError("The neutral face is no longer a planar face of its body",
                                      "draft-neutral-missing");
    const auto& plane = neutral->plane;
    try {
        return kernel::ShapeOperations::draftWithHistory(
            body, draftFaces(origins, parameters), {plane.originX, plane.originY, plane.originZ},
            {-plane.normalX, -plane.normalY, -plane.normalZ}, parameters.angle.value());
    } catch (const kernel::DraftFaceError& error) {
        throw GeometryEvaluationError("This face cannot take the draft", "draft-face-refused",
                                      {error.face() + 1});
    }
}

} // namespace

void evaluateDraft(const document::DocumentSnapshot& document,
                   const document::FeatureDescriptor& descriptor,
                   std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                   std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.draftParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the draft has no solid before it");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto evaluated = tilt(body.shape, owner, owner.supportOrigins, parameters);
    EvaluatedFeature output{
        descriptor.id,
        evaluated.shape,
        referenceBody.shape,
        body.bodyId,
        continueSupportOrigins(owner.supportOrigins, evaluated, kernel::HistoryOperand::first),
        {},
        owner.referenceSupportOrigins,
        continueEdgeOrigins(owner.edgeOrigins, evaluated, kernel::HistoryOperand::first),
        owner.referenceEdgeOrigins};
    // The unplaced reference gets the same draft (about its own neutral plane); where it
    // cannot, it stays as it was.
    try {
        EvaluatedFeature referenceOwner = owner;
        referenceOwner.shape = owner.referenceShape;
        referenceOwner.supportOrigins = owner.referenceSupportOrigins;
        const auto reference =
            tilt(referenceBody.shape, referenceOwner, owner.referenceSupportOrigins, parameters);
        output.referenceShape = reference.shape;
        output.referenceSupportOrigins = continueSupportOrigins(
            owner.referenceSupportOrigins, reference, kernel::HistoryOperand::first);
        output.referenceEdgeOrigins = continueEdgeOrigins(owner.referenceEdgeOrigins, reference,
                                                          kernel::HistoryOperand::first);
    } catch (const kernel::KernelError&) {
    } catch (const GeometryEvaluationError&) {
    }
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = output.referenceShape;
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
