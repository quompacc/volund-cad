// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/EdgeQueries.hpp>

#include "OcctGuard.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pln.hxx>

namespace voelund::kernel {
namespace {
// A raw OCCT failure as KernelError(evaluation): the message the evaluation gave before
// K-GRENZE, and the catches in the features see it.
constexpr auto operation = KernelOperation::evaluation;

bool sameCurve(const TopoDS_Edge& first, const TopoDS_Edge& second) {
    TopLoc_Location firstLocation;
    TopLoc_Location secondLocation;
    double firstStart = 0, firstEnd = 0, secondStart = 0, secondEnd = 0;
    const auto& firstCurve = BRep_Tool::Curve(first, firstLocation, firstStart, firstEnd);
    const auto& secondCurve = BRep_Tool::Curve(second, secondLocation, secondStart, secondEnd);
    return !firstCurve.IsNull() && firstCurve == secondCurve &&
           firstLocation.IsEqual(secondLocation);
}

EdgeCurve described(const TopoDS_Edge& edge) {
    const BRepAdaptor_Curve curve(edge);
    EdgeCurve result;
    result.range = {curve.FirstParameter(), curve.LastParameter()};
    result.start = curve.Value(result.range.first);
    result.end = curve.Value(result.range.last);
    if (curve.GetType() == GeomAbs_Line) {
        result.kind = EdgeCurve::Kind::line;
        result.line = curve.Line();
    } else if (curve.GetType() == GeomAbs_Circle) {
        result.kind = EdgeCurve::Kind::circle;
        result.circle = curve.Circle();
    }
    return result;
}
} // namespace

ParameterRange curveRange(const TopoDS_Edge& edge) {
    return detail::guarded(operation, [&] {
        const BRepAdaptor_Curve curve(edge);
        return ParameterRange{curve.FirstParameter(), curve.LastParameter()};
    });
}

ParameterRange storedRange(const TopoDS_Edge& edge) {
    return detail::guarded(operation, [&] {
        ParameterRange range{};
        BRep_Tool::Range(edge, range.first, range.last);
        return range;
    });
}

gp_Pnt pointOnEdge(const TopoDS_Edge& edge, double parameter) {
    return detail::guarded(operation, [&] { return BRepAdaptor_Curve(edge).Value(parameter); });
}

bool sameUnderlyingCurve(const TopoDS_Edge& first, const TopoDS_Edge& second) {
    return detail::guarded(operation, [&] { return sameCurve(first, second); });
}

TopoDS_Edge edgeOnSameCurve(const TopoDS_Edge& edge, double first, double last) {
    return detail::guarded(operation, [&] {
        TopLoc_Location location;
        double start = 0, end = 0;
        const auto shared = BRep_Tool::Curve(edge, location, start, end);
        TopoDS_Edge result = BRepBuilderAPI_MakeEdge(shared, first, last);
        result.Location(location);
        return result;
    });
}

EdgeCurve describeEdge(const TopoDS_Edge& edge) {
    return detail::guarded(operation, [&] { return described(edge); });
}

TopoDS_Face planePatch(const gp_Ax3& frame, double halfSize) {
    return detail::guarded(operation, [&] {
        return BRepBuilderAPI_MakeFace(gp_Pln(frame), -halfSize, halfSize, -halfSize, halfSize)
            .Face();
    });
}

TopoDS_Edge segmentEdge(const gp_Pnt& from, const gp_Pnt& to) {
    return detail::guarded(operation, [&] { return BRepBuilderAPI_MakeEdge(from, to).Edge(); });
}

MovedCopy::MovedCopy(const TopoDS_Shape& shape, const gp_Trsf& transform)
    : moved_(detail::guarded(
          operation,
          [&] { return std::make_shared<BRepBuilderAPI_Transform>(shape, transform, true); })),
      shape_(moved_->Shape()) {}

TopoDS_Shape MovedCopy::image(const TopoDS_Shape& part) const {
    return detail::guarded(operation, [&] { return moved_->ModifiedShape(part); });
}

} // namespace voelund::kernel
