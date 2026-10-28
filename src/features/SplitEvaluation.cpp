// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"
#include "StepEffects.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

// N12a (agent decision 2026-10-28): a body cut in two at a plane as it was before the split.
// The split keeps the front side, the back side or (both) the front, and its part is the back
// as a body of its own. Faces keep their names on either side; the part's faces carry
// {split, 1} so that both halves stay apart. The faces made by the cut stay unnamed.
namespace voelund::features::detail {
namespace {

// One side of a named body; a plane that leaves nothing on that side does not cut it.
NamedBody sideOf(const NamedBody& body, const CuttingPlane& plane, bool front) {
    try {
        const auto common = kernel::ShapeOperations::intersectWithHistory(
            body.shape,
            kernel::ShapeFactory::sideBlock(body.shape, plane.origin, plane.normal, front));
        return {common.shape,
                continueSupportOrigins(body.faces, common, kernel::HistoryOperand::first),
                continueEdgeOrigins(body.edges, common, kernel::HistoryOperand::first)};
    } catch (const kernel::KernelError& error) {
        if (error.failure() != kernel::BooleanFailure::emptyIntersection)
            throw;
        throw GeometryEvaluationError("The plane does not cut the body", "split-misses");
    }
}

// The body before the split and the split's plane.
struct SplitInput final {
    const EvaluatedFeature& owner;
    document::FeatureId producer;
    CuttingPlane plane;
};

SplitInput splitInput(const document::DocumentSnapshot& document, document::FeatureId split,
                      const std::vector<EvaluatedFeature>& outputs) {
    const auto& parameters = document.splitParameters(split);
    const auto producer = bodyProducerBefore(document, split, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the split has no solid before it",
                                      "split-body-missing");
    return {findFeature(outputs, *producer), *producer,
            cuttingPlane(document, split, parameters.plane, outputs, "split")};
}

} // namespace

void evaluateSplit(const document::DocumentSnapshot& document,
                   const document::FeatureDescriptor& descriptor,
                   std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                   std::vector<EvaluatedBody>& referenceBodies) {
    const auto input = splitInput(document, descriptor.id, outputs);
    const auto keep = document.splitParameters(descriptor.id).keep;
    const auto& owner = input.owner;
    EvaluatedBody& body = findBody(bodies, outputs, input.producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, input.producer);
    const NamedBody whole{body.shape, owner.supportOrigins, owner.edgeOrigins};
    // Both sides must hold material, whichever is kept.
    auto front = sideOf(whole, input.plane, true);
    auto back = sideOf(whole, input.plane, false);
    auto& kept = keep == document::SplitKeep::back ? back : front;
    EvaluatedFeature output{descriptor.id,
                            kept.shape,
                            referenceBody.shape,
                            body.bodyId,
                            kept.faces,
                            {},
                            owner.referenceSupportOrigins,
                            kept.edges,
                            owner.referenceEdgeOrigins};
    try {
        auto reference =
            sideOf({referenceBody.shape, owner.referenceSupportOrigins, owner.referenceEdgeOrigins},
                   input.plane, keep != document::SplitKeep::back);
        output.referenceShape = reference.shape;
        output.referenceSupportOrigins = std::move(reference.faces);
        output.referenceEdgeOrigins = std::move(reference.edges);
    } catch (const GeometryEvaluationError&) {
    } catch (const kernel::KernelError&) {
    }
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = output.referenceShape;
    outputs.push_back(std::move(output));
}

void evaluateSplitPart(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    const auto split = document.splitPartParameters(descriptor.id).split;
    const auto input = splitInput(document, split, outputs);
    auto back = sideOf({input.owner.shape, input.owner.supportOrigins, input.owner.edgeOrigins},
                       input.plane, false);
    for (auto& face : back.faces)
        face.origin.instances.push_back({split, 1});
    const document::BodyId bodyId{descriptor.id.value()};
    EvaluatedFeature output{descriptor.id, back.shape, back.shape, bodyId, back.faces, {},
                            back.faces,    back.edges, back.edges};
    bodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.shape});
    referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.referenceShape});
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
