// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"
#include <voelund/kernel/PlaneNormal.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/HoleTool.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <voelund/kernel/ShapeQueries.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

// M08 (user decision 2026-10-13): a hole into a planar body face, at (x, y) in the face's
// frame, along the inward normal. The face is named by its origin and followed through the
// body's history; a hole "through all" reaches through the body however thick it becomes.
// The hole's wall and bottom are named like a cylinder's (cylinderSide, cylinderEnd).
namespace voelund::features::detail {
namespace {

EvaluatedFeature referenceOf(const EvaluatedFeature& output) {
    return {output.id,
            output.referenceShape,
            output.referenceShape,
            output.bodyId,
            output.referenceSupportOrigins,
            {},
            {},
            output.referenceEdgeOrigins};
}

// N09: the turned profile of a counterbore, countersink or drill point (a plain hole keeps
// M08's cylinder, so its faces keep their names).
kernel::ShapeOperationResult variantTool(const document::HoleParameters& parameters,
                                         const gp_Pnt& centre, const gp_Vec& inward,
                                         double length) {
    const auto& variant = parameters.variant;
    const double degrees = std::numbers::pi / 180.0;
    kernel::HoleProfile profile{parameters.diameter / 2.0, length};
    if (variant.kind != document::HoleKind::simple)
        profile.headRadius = variant.headDiameter / 2.0;
    if (variant.kind == document::HoleKind::counterbore)
        profile.headDepth = variant.headDepth;
    profile.countersinkAngle = variant.countersinkAngle * degrees;
    if (variant.drillPoint)
        profile.tipAngle = document::drillPointAngle * degrees;
    profile.origin = {centre.X(), centre.Y(), centre.Z()};
    const gp_Dir direction(inward);
    profile.direction = {direction.X(), direction.Y(), direction.Z()};
    try {
        return kernel::makeHoleToolWithHistory(profile);
    } catch (const kernel::KernelError&) {
        throw GeometryEvaluationError("The head of the hole is deeper than the hole",
                                      "hole-head-too-deep");
    }
}

// The cylinder the hole removes from `body`, whose current faces `owner` names.
kernel::ShapeOperationResult holeTool(const EvaluatedFeature& owner, const TopoDS_Shape& body,
                                      const document::HoleParameters& parameters) {
    const auto frames = supportFaceFrames(owner);
    const auto frame = std::ranges::find(frames, parameters.face.origin, &SupportFaceFrame::origin);
    if (frame == frames.end())
        throw GeometryEvaluationError(
            "The face of the hole is no longer a unique planar face of its body",
            "hole-face-missing");
    const auto& plane = frame->plane;
    const gp_Vec normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Vec xAxis(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec yAxis = normal.Crossed(xAxis);
    gp_Pnt centre(plane.originX, plane.originY, plane.originZ);
    centre.Translate(xAxis * parameters.x + yAxis * parameters.y);
    // Into the body: against the outward normal of the face as it lies in the solid.
    gp_Vec inward;
    for (TopExp_Explorer explorer(body, TopAbs_FACE); explorer.More(); explorer.Next()) {
        if (!explorer.Current().IsSame(frame->face))
            continue;
        if (const auto outward = kernel::planarFaceNormal(explorer.Current()))
            inward = -gp_Vec(*outward);
        break;
    }
    if (inward.Magnitude() < 0.5)
        throw GeometryEvaluationError("The face of the hole is not a face of its body",
                                      "hole-face-missing");
    double length = parameters.depth.value_or(0.0);
    if (!parameters.depth) {
        // Through all: longer than the body could ever be measured from this face.
        const auto box = kernel::boundingBox(body);
        length = 2.0 * (box ? box->diagonal() : 0.0) + 1.0;
    }
    if (!parameters.variant.plain())
        return variantTool(parameters, centre, inward, length);
    return kernel::ShapeFactory::makeCylinderWithHistory({parameters.diameter / 2.0, length,
                                                          centre.X(), centre.Y(), centre.Z(),
                                                          inward.X(), inward.Y(), inward.Z()});
}

std::vector<SupportProvenance> joined(std::span<const SupportProvenance> previous,
                                      std::span<const SupportProvenance> tool,
                                      const kernel::ShapeOperationResult& result) {
    auto origins = continueSupportOrigins(previous, result, kernel::HistoryOperand::first);
    auto added = continueSupportOrigins(tool, result, kernel::HistoryOperand::second);
    origins.insert(origins.end(), added.begin(), added.end());
    return origins;
}
std::vector<EdgeProvenance> joinedEdges(std::span<const EdgeProvenance> previous,
                                        std::span<const EdgeProvenance> tool,
                                        const kernel::ShapeOperationResult& result) {
    auto edges = continueEdgeOrigins(previous, result, kernel::HistoryOperand::first);
    auto added = continueEdgeOrigins(tool, result, kernel::HistoryOperand::second);
    edges.insert(edges.end(), added.begin(), added.end());
    return edges;
}

} // namespace

void evaluateHole(const document::DocumentSnapshot& document,
                  const document::FeatureDescriptor& descriptor,
                  std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                  std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.holeParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.face.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the hole has no solid before it",
                                      "body-missing");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto tool = holeTool(owner, body.shape, parameters);
    const auto referenceOwner = referenceOf(owner);
    const auto referenceTool = holeTool(referenceOwner, referenceBody.shape, parameters);
    const auto evaluated = kernel::ShapeOperations::cutWithHistory(body.shape, tool.shape);
    const auto reference =
        kernel::ShapeOperations::cutWithHistory(referenceBody.shape, referenceTool.shape);
    outputs.push_back(EvaluatedFeature{
        descriptor.id,
        evaluated.shape,
        reference.shape,
        body.bodyId,
        joined(owner.supportOrigins, primitiveSupportOrigins(descriptor.id, tool), evaluated),
        {},
        joined(owner.referenceSupportOrigins, primitiveSupportOrigins(descriptor.id, referenceTool),
               reference),
        joinedEdges(owner.edgeOrigins, primitiveEdgeOrigins(descriptor.id, tool), evaluated),
        joinedEdges(owner.referenceEdgeOrigins, primitiveEdgeOrigins(descriptor.id, referenceTool),
                    reference)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
