// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

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

document::SketchPlane referencePlane(const document::DocumentSnapshot& document,
                                     document::FeatureId feature,
                                     const document::DatumPlaneParameters& parameters,
                                     const std::vector<EvaluatedFeature>& outputs) {
    if (parameters.basePlane)
        return document::datumSketchPlane(*parameters.basePlane);
    const auto producer = bodyProducerBefore(document, feature, parameters.face->body);
    if (!producer)
        throw GeometryEvaluationError("The body of the construction plane has no solid",
                                      "datum-reference-missing");
    const auto frames = supportFaceFrames(findFeature(outputs, *producer));
    const auto found =
        std::ranges::find(frames, parameters.face->origin, &SupportFaceFrame::origin);
    if (found == frames.end())
        throw GeometryEvaluationError(
            "The face of the construction plane is no longer a unique planar face",
            "datum-reference-missing");
    return found->plane;
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
    const BRepAdaptor_Curve curve(edges.front());
    if (curve.GetType() == GeomAbs_Line)
        return {curve.Value(curve.FirstParameter()), curve.Line().Direction()};
    if (curve.GetType() == GeomAbs_Circle)
        return {curve.Circle().Location(), curve.Circle().Axis().Direction()};
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
    const auto face = BRepBuilderAPI_MakeFace(gp_Pln(gp_Ax3(placed, normal, x)), -shownHalfSize,
                                              shownHalfSize, -shownHalfSize, shownHalfSize)
                          .Face();
    EvaluatedFeature output{descriptor.id, face, face, std::nullopt};
    output.datumFrame = frame;
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
    const auto edge = BRepBuilderAPI_MakeEdge(origin.Translated(gp_Vec(direction) * -shownHalfSize),
                                              origin.Translated(gp_Vec(direction) * shownHalfSize))
                          .Edge();
    EvaluatedFeature output{descriptor.id, edge, edge, std::nullopt};
    output.datumFrame = frameOf(origin, direction, across);
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
