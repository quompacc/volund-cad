// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <algorithm>

// M22 (user decision 2026-10-14): hollows a body to walls of one thickness, with the named
// faces opened. The faces are found by their names on the body before the shell; the faces
// that stay keep their names.
namespace voelund::features::detail {
namespace {

std::vector<TopoDS_Shape> openFaces(std::span<const SupportProvenance> origins,
                                    const document::ShellParameters& parameters) {
    std::vector<TopoDS_Shape> faces;
    for (const auto& origin : parameters.openFaces) {
        const auto found = std::ranges::find(origins, origin, &SupportProvenance::origin);
        if (found == origins.end() || found->faces.empty())
            throw GeometryEvaluationError("A face the shell opens is no longer on its body",
                                          "shell-face-missing");
        faces.insert(faces.end(), found->faces.begin(), found->faces.end());
    }
    return faces;
}

} // namespace

void evaluateShell(const document::DocumentSnapshot& document,
                   const document::FeatureDescriptor& descriptor,
                   std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                   std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.shellParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the shell has no solid before it",
                                      "body-missing");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto evaluated = kernel::ShapeOperations::shellWithHistory(
        body.shape, openFaces(owner.supportOrigins, parameters), parameters.thickness,
        parameters.outward);
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
    // The unplaced reference gets the same walls; where it cannot, it stays as it was.
    try {
        const auto reference = kernel::ShapeOperations::shellWithHistory(
            referenceBody.shape, openFaces(owner.referenceSupportOrigins, parameters),
            parameters.thickness, parameters.outward);
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
