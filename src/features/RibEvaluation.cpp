// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"
#include "StepEffects.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/Rib.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <gp_Vec.hxx>

#include <optional>
#include <variant>

// N12b (agent decision 2026-10-28): a rib joins the material between its sketch's one straight
// line and the faces of its target body it meets. The body's faces keep their names; the rib's
// own faces stay unnamed.
namespace voelund::features::detail {
namespace {

// The sketch's one drawn straight line in model coordinates, and where the rib grows.
kernel::RibLine ribLine(const document::SketchParameters& sketch, bool flipped) {
    std::optional<document::SketchLine> line;
    for (const auto& element : sketch.elements.values) {
        if (element.construction)
            continue;
        const auto* straight = std::get_if<document::SketchLine>(&element.geometry);
        if (straight == nullptr || line)
            throw GeometryEvaluationError("A rib needs exactly one straight line in its sketch",
                                          "rib-line");
        line = *straight;
    }
    if (!line)
        throw GeometryEvaluationError("A rib needs exactly one straight line in its sketch",
                                      "rib-line");
    const auto& plane = sketch.plane;
    const gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir y = normal.Crossed(x);
    const auto at = [&](const document::SketchPoint& point) {
        return origin.Translated(gp_Vec(x) * point.x + gp_Vec(y) * point.y);
    };
    const gp_Pnt start = at(line->start);
    const gp_Pnt end = at(line->end);
    if (start.Distance(end) <= 1e-9)
        throw GeometryEvaluationError("The rib's line has no length", "rib-line");
    gp_Dir side = normal.Crossed(gp_Dir(gp_Vec(start, end)));
    if (flipped)
        side.Reverse();
    return {start, end, normal, side};
}

NamedBody withRib(const NamedBody& body, const kernel::RibLine& line, double thickness) {
    try {
        const auto fused = kernel::ShapeOperations::fuseWithHistory(
            body.shape, kernel::ribMaterial(body.shape, line, thickness));
        return {fused.shape,
                continueSupportOrigins(body.faces, fused, kernel::HistoryOperand::first),
                continueEdgeOrigins(body.edges, fused, kernel::HistoryOperand::first)};
    } catch (const kernel::KernelError& error) {
        if (error.operation() != kernel::KernelOperation::makePrism)
            throw;
        throw GeometryEvaluationError("The rib meets no faces of its body on that side",
                                      "rib-open");
    }
}

} // namespace

void evaluateRib(const document::DocumentSnapshot& document,
                 const document::FeatureDescriptor& descriptor,
                 std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                 std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.extrusionParameters(descriptor.id);
    if (!descriptor.inputFeature)
        throw GeometryEvaluationError("A rib requires a sketch input");
    const auto& sketch = findFeature(outputs, *descriptor.inputFeature).resolvedSketch;
    if (!sketch)
        throw GeometryEvaluationError("Current sketch projection is missing");
    const auto line = ribLine(*sketch, parameters.rib->flipped);
    const auto producer = bodyProducerBefore(document, descriptor.id, *parameters.target);
    if (!producer)
        throw GeometryEvaluationError("The rib's body has no solid before it", "body-missing");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const double thickness = parameters.rib->thickness;
    const auto current =
        withRib({body.shape, owner.supportOrigins, owner.edgeOrigins}, line, thickness);
    EvaluatedFeature output{descriptor.id,
                            current.shape,
                            referenceBody.shape,
                            body.bodyId,
                            current.faces,
                            {},
                            owner.referenceSupportOrigins,
                            current.edges,
                            owner.referenceEdgeOrigins};
    try {
        auto reference = withRib(
            {referenceBody.shape, owner.referenceSupportOrigins, owner.referenceEdgeOrigins}, line,
            thickness);
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

} // namespace voelund::features::detail
