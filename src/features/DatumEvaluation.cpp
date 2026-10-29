// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/Measurement.hpp>
#include <voelund/kernel/ShapeQueries.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <gp_Vec.hxx>
#include <span>

#include <algorithm>

// M19 (user decision 2026-10-14): construction planes and axes. They belong to no body; their
// result is a frame (EvaluatedFeature::datumFrame) and a small shape to show. A plane on a
// face and an axis on an edge follow their body through its history.
namespace voelund::features::detail {
namespace {

constexpr double shownHalfSize = 30.0;

document::SketchPlane frameOf(const gp_Pnt& origin, const gp_Dir& normal, const gp_Dir& x) {
    return {origin.X(), origin.Y(), origin.Z(), normal.X(), normal.Y(),
            normal.Z(), x.X(),      x.Y(),      x.Z()};
}

document::SketchPlane facePlane(const document::DocumentSnapshot& document,
                                document::FeatureId feature,
                                const document::BodyFaceReference& face,
                                const std::vector<EvaluatedFeature>& outputs) {
    const auto producer = bodyProducerBefore(document, feature, face.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the construction plane has no solid",
                                      "datum-reference-missing");
    const auto frames = supportFaceFrames(findFeature(outputs, *producer));
    const auto found = std::ranges::find(frames, face.origin, &SupportFaceFrame::origin);
    if (found == frames.end())
        throw GeometryEvaluationError(
            "The face of the construction plane is no longer a unique planar face",
            "datum-reference-missing");
    return found->plane;
}

gp_Pnt originOf(const document::SketchPlane& frame) {
    return {frame.originX, frame.originY, frame.originZ};
}
gp_Dir normalOf(const document::SketchPlane& frame) {
    return {frame.normalX, frame.normalY, frame.normalZ};
}

// N13b: halfway between two parallel faces, facing as the first.
document::SketchPlane midplane(const document::SketchPlane& first,
                               const document::SketchPlane& second) {
    const gp_Dir normal = normalOf(first);
    if (gp_Vec(normal).Crossed(gp_Vec(normalOf(second))).Magnitude() > 1e-9)
        throw GeometryEvaluationError("A midplane lies between two parallel faces",
                                      "datum-midplane-not-parallel");
    const double gap = gp_Vec(originOf(first), originOf(second)).Dot(gp_Vec(normal));
    const gp_Pnt middle = originOf(first).Translated(gp_Vec(normal) * (gap / 2));
    return frameOf(middle, normal, gp_Dir(first.xDirectionX, first.xDirectionY, first.xDirectionZ));
}

// N13b: through three construction points, from the first, towards the second its x.
document::SketchPlane planeThrough(std::span<const document::FeatureId> points,
                                   const std::vector<EvaluatedFeature>& outputs) {
    std::vector<gp_Pnt> at;
    for (const auto point : points) {
        const auto& frame = findFeature(outputs, point).datumFrame;
        if (!frame)
            throw GeometryEvaluationError("A point of the plane is missing",
                                          "datum-reference-missing");
        at.push_back(originOf(*frame));
    }
    const gp_Vec x(at[0], at[1]);
    const gp_Vec across = x.Crossed(gp_Vec(at[0], at[2]));
    if (x.Magnitude() < 1e-9 || across.Magnitude() < 1e-9 * x.Magnitude())
        throw GeometryEvaluationError("Three points on one line span no plane",
                                      "datum-points-collinear");
    return frameOf(at[0], gp_Dir(across), gp_Dir(x));
}

document::SketchPlane referencePlane(const document::DocumentSnapshot& document,
                                     document::FeatureId feature,
                                     const document::DatumPlaneParameters& parameters,
                                     const std::vector<EvaluatedFeature>& outputs) {
    if (!parameters.points.empty())
        return planeThrough(parameters.points, outputs);
    if (parameters.basePlane)
        return document::datumSketchPlane(*parameters.basePlane);
    const auto first = facePlane(document, feature, *parameters.face, outputs);
    if (!parameters.secondFace)
        return first;
    return midplane(first, facePlane(document, feature, *parameters.secondFace, outputs));
}

// N13b: the place on a construction point's edge.
gp_Pnt edgeSpot(const document::DocumentSnapshot& document, document::FeatureId feature,
                const document::DatumPointParameters& parameters,
                const std::vector<EvaluatedFeature>& outputs) {
    const auto producer = bodyProducerBefore(document, feature, *parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the point has no solid",
                                      "datum-reference-missing");
    const auto edges =
        filletEdgeShapes(findFeature(outputs, *producer).supportOrigins, *parameters.edge);
    if (edges.size() != 1)
        throw GeometryEvaluationError("The edge of the point is gone", "datum-reference-missing");
    const auto curve = kernel::describeEdge(edges.front());
    switch (parameters.spot) {
    case document::EdgeSpot::start:
        return curve.start;
    case document::EdgeSpot::end:
        return curve.end;
    case document::EdgeSpot::middle:
        return kernel::boundingBoxCentre(
            kernel::edgePoint(edges.front(), kernel::EdgePoint::middle));
    default:
        if (curve.kind != kernel::EdgeCurve::Kind::circle)
            throw GeometryEvaluationError("Only a circular edge has a centre",
                                          "datum-point-centre");
        return curve.circle.Location();
    }
}

} // namespace

AxisLine datumAxisLine(const document::DocumentSnapshot& document, document::FeatureId feature,
                       const document::DatumAxisParameters& parameters,
                       const std::vector<EvaluatedFeature>& outputs) {
    if (parameters.baseAxis)
        return {gp_Pnt(0, 0, 0), *parameters.baseAxis == document::BaseAxis::x   ? gp_Dir(1, 0, 0)
                                 : *parameters.baseAxis == document::BaseAxis::y ? gp_Dir(0, 1, 0)
                                                                                 : gp_Dir(0, 0, 1)};
    const auto producer = bodyProducerBefore(document, feature, *parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the axis has no solid",
                                      "datum-reference-missing");
    const auto edges =
        filletEdgeShapes(findFeature(outputs, *producer).supportOrigins, *parameters.edge);
    if (edges.size() != 1)
        throw GeometryEvaluationError("The edge of the axis is gone", "datum-reference-missing");
    const auto curve = kernel::describeEdge(edges.front());
    if (curve.kind == kernel::EdgeCurve::Kind::line)
        return {curve.start, curve.line.Direction()};
    if (curve.kind == kernel::EdgeCurve::Kind::circle)
        return {curve.circle.Location(), curve.circle.Axis().Direction()};
    throw GeometryEvaluationError("An axis lies on a straight or circular edge", "datum-axis-edge");
}

void evaluateDatumPlane(const document::DocumentSnapshot& document,
                        const document::FeatureDescriptor& descriptor,
                        std::vector<EvaluatedFeature>& outputs) {
    const auto& parameters = document.datumPlaneParameters(descriptor.id);
    const auto reference = referencePlane(document, descriptor.id, parameters, outputs);
    const gp_Pnt origin(reference.originX, reference.originY, reference.originZ);
    gp_Dir normal(reference.normalX, reference.normalY, reference.normalZ);
    gp_Dir x(reference.xDirectionX, reference.xDirectionY, reference.xDirectionZ);
    if (parameters.rotationAxis != document::DatumRotationAxis::none) {
        const gp_Dir axis = parameters.rotationAxis == document::DatumRotationAxis::x
                                ? x
                                : normal.Crossed(x); // the frame's y
        const gp_Ax1 turn(origin, axis);
        normal.Rotate(turn, parameters.angle.value());
        x.Rotate(turn, parameters.angle.value());
    }
    const gp_Pnt placed = origin.Translated(gp_Vec(normal) * parameters.offset);
    const auto frame = frameOf(placed, normal, x);
    const auto face = kernel::planePatch(gp_Ax3(placed, normal, x), shownHalfSize);
    EvaluatedFeature output{descriptor.id, face, face, std::nullopt};
    output.datumFrame = frame;
    outputs.push_back(std::move(output));
}

void evaluateDatumPoint(const document::DocumentSnapshot& document,
                        const document::FeatureDescriptor& descriptor,
                        std::vector<EvaluatedFeature>& outputs) {
    const auto& parameters = document.datumPointParameters(descriptor.id);
    const auto& position = parameters.position;
    const gp_Pnt point = position ? gp_Pnt((*position)[0], (*position)[1], (*position)[2])
                                  : edgeSpot(document, descriptor.id, parameters, outputs);
    const auto vertex = kernel::pointVertex(point);
    EvaluatedFeature output{descriptor.id, vertex, vertex, std::nullopt};
    output.datumFrame = frameOf(point, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    outputs.push_back(std::move(output));
}

void evaluateDatumAxis(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs) {
    const auto [origin, direction] = datumAxisLine(
        document, descriptor.id, document.datumAxisParameters(descriptor.id), outputs);
    // Any direction across the axis completes the frame.
    const gp_Dir across = std::abs(direction.Z()) < 0.9 ? gp_Dir(0, 0, 1).Crossed(direction)
                                                        : gp_Dir(1, 0, 0).Crossed(direction);
    const auto edge = kernel::segmentEdge(origin.Translated(gp_Vec(direction) * -shownHalfSize),
                                          origin.Translated(gp_Vec(direction) * shownHalfSize));
    EvaluatedFeature output{descriptor.id, edge, edge, std::nullopt};
    output.datumFrame = frameOf(origin, direction, across);
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
