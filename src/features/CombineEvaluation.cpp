// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <algorithm>

// M21 (user decision 2026-10-14): a Boolean of whole bodies. The target body takes the result
// and keeps its identity; each tool body is applied in order. Faces and edges of all bodies
// keep their names, so sketches, fillets and holes on them follow. Tools are consumed unless
// kept: they are then no separate result, but their features stay editable.
namespace voelund::features::detail {
namespace {

kernel::ShapeOperationResult apply(document::CombineOperation operation, const TopoDS_Shape& target,
                                   const TopoDS_Shape& tool) {
    switch (operation) {
    case document::CombineOperation::join:
        return kernel::ShapeOperations::fuseWithHistory(target, tool);
    case document::CombineOperation::cut:
        return kernel::ShapeOperations::cutWithHistory(target, tool);
    case document::CombineOperation::intersect:
        break;
    }
    return kernel::ShapeOperations::intersectWithHistory(target, tool);
}

struct Combined final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};

// Applies one tool to the running result and carries both operands' names through.
void combine(Combined& result, document::CombineOperation operation, const TopoDS_Shape& tool,
             std::span<const SupportProvenance> toolFaces,
             std::span<const EdgeProvenance> toolEdges) {
    const auto evaluated = apply(operation, result.shape, tool);
    auto faces = continueSupportOrigins(result.faces, evaluated, kernel::HistoryOperand::first);
    const auto addedFaces =
        continueSupportOrigins(toolFaces, evaluated, kernel::HistoryOperand::second);
    faces.insert(faces.end(), addedFaces.begin(), addedFaces.end());
    auto edges = continueEdgeOrigins(result.edges, evaluated, kernel::HistoryOperand::first);
    const auto addedEdges =
        continueEdgeOrigins(toolEdges, evaluated, kernel::HistoryOperand::second);
    edges.insert(edges.end(), addedEdges.begin(), addedEdges.end());
    result = {evaluated.shape, std::move(faces), std::move(edges)};
}

} // namespace

void evaluateCombine(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.combineParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.target);
    if (!producer)
        throw GeometryEvaluationError("The target body of the combination has no solid",
                                      "combine-body-missing");
    const auto& owner = findFeature(outputs, *producer);
    Combined current{findBody(bodies, outputs, *producer).shape, owner.supportOrigins,
                     owner.edgeOrigins};
    Combined reference{findBody(referenceBodies, outputs, *producer).shape,
                       owner.referenceSupportOrigins, owner.referenceEdgeOrigins};
    for (const auto tool : parameters.tools) {
        const auto toolProducer = bodyProducerBefore(document, descriptor.id, tool);
        if (!toolProducer)
            throw GeometryEvaluationError("A tool body of the combination has no solid",
                                          "combine-body-missing");
        const auto& toolOwner = findFeature(outputs, *toolProducer);
        combine(current, parameters.operation, findBody(bodies, outputs, *toolProducer).shape,
                toolOwner.supportOrigins, toolOwner.edgeOrigins);
        // Reference results are unplaced primitives: there the bodies may not meet at all
        // (two cylinders both at the origin). The reference then keeps the target without
        // this tool; only sketches on the tool's faces need the reference of those faces.
        try {
            combine(reference, parameters.operation,
                    findBody(referenceBodies, outputs, *toolProducer).shape,
                    toolOwner.referenceSupportOrigins, toolOwner.referenceEdgeOrigins);
        } catch (const kernel::KernelError&) {
        }
    }
    EvaluatedFeature output{descriptor.id,
                            current.shape,
                            reference.shape,
                            parameters.target,
                            std::move(current.faces),
                            {},
                            std::move(reference.faces),
                            std::move(current.edges),
                            std::move(reference.edges)};
    if (!parameters.keepTools)
        output.consumedBodies = parameters.tools;
    const auto update = [&](std::vector<EvaluatedBody>& list, const TopoDS_Shape& shape) {
        auto& body = findBody(list, outputs, *producer);
        body.sourceFeature = descriptor.id;
        body.shape = shape;
        for (const auto consumed : output.consumedBodies)
            std::erase_if(list, [&](const auto& item) { return item.bodyId == consumed; });
    };
    update(bodies, output.shape);
    update(referenceBodies, output.referenceShape);
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
