// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchPlaneGuard.hpp"
#include <voelund/features/SketchSupportResolver.hpp>
#include <voelund/kernel/PlaneNormal.hpp>

#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>
#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace voelund::features {
namespace {

struct ResolvedFace final {
    TopoDS_Face face;
    document::SketchPlane plane;
};

struct ReferenceSegment final {
    double firstX;
    double firstY;
    double secondX;
    double secondY;
};

std::optional<ResolvedFace> resolveSupportFace(const document::SketchPlane& previous,
                                               const TopoDS_Shape& supportShape) {
    std::optional<ResolvedFace> best;
    double bestDistance = std::numeric_limits<double>::max();
    const gp_Vec previousNormal(previous.normalX, previous.normalY, previous.normalZ);
    const gp_Pnt previousOrigin(previous.originX, previous.originY, previous.originZ);
    for (TopExp_Explorer explorer(supportShape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        const auto planar = kernel::facePlane(face);
        if (!planar)
            continue;
        const gp_Pln candidate = *planar;
        const gp_Dir normal = kernel::planeNormal(candidate, face.Orientation());
        if (previousNormal.Dot(gp_Vec(normal)) < 0.999)
            continue;
        const double signedDistance =
            gp_Vec(previousOrigin, candidate.Location()).Dot(previousNormal);
        const double distance = std::abs(signedDistance);
        if (distance >= bestDistance)
            continue;
        bestDistance = distance;
        const gp_Pnt origin = previousOrigin.Translated(previousNormal * signedDistance);
        best = ResolvedFace{face,
                            {origin.X(), origin.Y(), origin.Z(), previous.normalX, previous.normalY,
                             previous.normalZ, previous.xDirectionX, previous.xDirectionY,
                             previous.xDirectionZ}};
    }
    return best;
}

std::vector<ReferenceSegment> referenceSegments(const ResolvedFace& support) {
    const gp_Pnt origin(support.plane.originX, support.plane.originY, support.plane.originZ);
    const gp_Vec normal(support.plane.normalX, support.plane.normalY, support.plane.normalZ);
    const gp_Vec xDirection(support.plane.xDirectionX, support.plane.xDirectionY,
                            support.plane.xDirectionZ);
    const gp_Vec yDirection = normal.Crossed(xDirection);
    std::vector<ReferenceSegment> result;
    for (TopExp_Explorer explorer(support.face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        const auto curve = kernel::describeEdge(edge);
        const int divisions = curve.kind == kernel::EdgeCurve::Kind::line ? 1 : 12;
        const double firstParameter = curve.range.first;
        const double lastParameter = curve.range.last;
        gp_Pnt first = curve.start;
        for (int index = 1; index <= divisions; ++index) {
            const gp_Pnt second = kernel::pointOnEdge(
                edge, firstParameter + (lastParameter - firstParameter) * index / divisions);
            const gp_Vec firstOffset(origin, first);
            const gp_Vec secondOffset(origin, second);
            result.push_back({firstOffset.Dot(xDirection), firstOffset.Dot(yDirection),
                              secondOffset.Dot(xDirection), secondOffset.Dot(yDirection)});
            first = second;
        }
    }
    return result;
}

} // namespace

document::SketchPlane resolveSketchPlane(const document::SketchPlane& previous,
                                         const TopoDS_Shape& supportShape) {
    const auto support = resolveSupportFace(previous, supportShape);
    return support.has_value() ? support->plane : previous;
}

bool updateSketchSupportReferences(document::SketchParameters& sketch,
                                   const TopoDS_Shape& supportShape) {
    detail::requireCanonicalPlane(sketch.plane);
    const auto support = resolveSupportFace(sketch.plane, supportShape);
    if (!support.has_value())
        return false;
    sketch.plane = support->plane;
    const auto segments = referenceSegments(*support);
    for (auto& constraint : sketch.constraints.values) {
        if ((constraint.kind != document::SketchConstraintKind::coordinateX &&
             constraint.kind != document::SketchConstraintKind::coordinateY) ||
            constraint.auxiliaryValues.size() != 2)
            continue;
        const double storedId = constraint.auxiliaryValues[0];
        const auto referenceId = static_cast<std::uint64_t>(std::llround(storedId));
        if (referenceId == 0 || referenceId > segments.size() ||
            std::abs(storedId - static_cast<double>(referenceId)) > 1.0e-9)
            return false;
        const double parameter = constraint.auxiliaryValues[1];
        const ReferenceSegment& segment = segments[referenceId - 1];
        const double x = segment.firstX + (segment.secondX - segment.firstX) * parameter;
        const double y = segment.firstY + (segment.secondY - segment.firstY) * parameter;
        constraint.value = constraint.kind == document::SketchConstraintKind::coordinateX ? x : y;
    }
    return true;
}

} // namespace voelund::features
