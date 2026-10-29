// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/FaceEdit.hpp>
#include <voelund/kernel/KernelError.hpp>

#include <algorithm>

// N13a (agent decision 2026-10-29): faces of a body moved along their normals or removed with
// the gap healed. The faces are found by their names on the body before the edit; faces that
// stay keep their names.
namespace voelund::features::detail {
namespace {

std::vector<TopoDS_Shape> editedFaces(std::span<const SupportProvenance> origins,
                                      const document::FaceEditParameters& parameters) {
    std::vector<TopoDS_Shape> faces;
    for (const auto& origin : parameters.faces) {
        const auto found = std::ranges::find(origins, origin, &SupportProvenance::origin);
        if (found == origins.end() || found->faces.empty())
            throw GeometryEvaluationError("An edited face is no longer on its body",
                                          "face-edit-face-missing");
        faces.insert(faces.end(), found->faces.begin(), found->faces.end());
    }
    return faces;
}

kernel::ShapeOperationResult edited(const TopoDS_Shape& body,
                                    std::span<const SupportProvenance> origins,
                                    const document::FaceEditParameters& parameters) {
    const auto faces = editedFaces(origins, parameters);
    try {
        return parameters.mode == document::FaceEditMode::offset
                   ? kernel::offsetFacesWithHistory(body, faces, parameters.distance)
                   : kernel::removeFacesWithHistory(body, faces);
    } catch (const kernel::KernelError& error) {
        throw GeometryEvaluationError(error.diagnostic(), "face-edit-failed");
    }
}

} // namespace

void evaluateFaceEdit(const document::DocumentSnapshot& document,
                      const document::FeatureDescriptor& descriptor,
                      std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                      std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.faceEditParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The edited body has no solid before it", "body-missing");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto evaluated = edited(body.shape, owner.supportOrigins, parameters);
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
    // The unplaced reference gets the same edit; where it cannot, it stays as it was.
    try {
        const auto reference =
            edited(referenceBody.shape, owner.referenceSupportOrigins, parameters);
        output.referenceShape = reference.shape;
        output.referenceSupportOrigins = continueSupportOrigins(
            owner.referenceSupportOrigins, reference, kernel::HistoryOperand::first);
        output.referenceEdgeOrigins = continueEdgeOrigins(owner.referenceEdgeOrigins, reference,
                                                          kernel::HistoryOperand::first);
    } catch (const GeometryEvaluationError&) {
    }
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = output.referenceShape;
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
