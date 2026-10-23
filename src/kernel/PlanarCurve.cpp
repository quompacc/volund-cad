// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OcctGuard.hpp"

#include <voelund/kernel/PlanarCurve.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <NCollection_Array1.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2d.hxx>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace voelund::kernel {

struct PlanarCurve::Impl final {
    Handle(Geom2d_Curve) curve;
};

namespace {
constexpr auto operation = KernelOperation::sketchGeometry;

double scaleOf(const gp_Pnt2d& point) { return std::max(std::abs(point.X()), std::abs(point.Y())); }

Handle(Geom2d_BSplineCurve) bsplineCurve(const BSpline2d& spline) {
    const int count = static_cast<int>(spline.poles.size());
    const int degree = bsplineDegree(spline.poles.size());
    NCollection_Array1<gp_Pnt2d> poles(1, count);
    for (int index = 0; index < count; ++index)
        poles.SetValue(index + 1, spline.poles[static_cast<std::size_t>(index)]);
    const int spans = static_cast<int>(bsplineLastParameter(spline.poles.size()));
    NCollection_Array1<double> knots(1, spans + 1);
    NCollection_Array1<int> multiplicities(1, spans + 1);
    for (int index = knots.Lower(); index <= knots.Upper(); ++index) {
        knots.SetValue(index, index - knots.Lower());
        multiplicities.SetValue(index,
                                index == knots.Lower() || index == knots.Upper() ? degree + 1 : 1);
    }
    return new Geom2d_BSplineCurve(poles, knots, multiplicities, degree);
}

struct Builder final {
    PlanarCurve& curve;
    Handle(Geom2d_Curve) & handle;

    void operator()(const Line2d& line) const {
        const double length = line.start.Distance(line.end);
        Handle(Geom2d_Curve) basis =
            new Geom2d_Line(line.start, gp_Dir2d(gp_Vec2d(line.start, line.end)));
        handle = new Geom2d_TrimmedCurve(basis, 0, length);
        curve.first = 0;
        curve.last = length;
        curve.scale = std::max(scaleOf(line.start), scaleOf(line.end));
    }
    void operator()(const Circle2d& circle) const {
        handle = new Geom2d_Circle(gp_Ax2d(circle.centre, gp_Dir2d(1, 0)), circle.radius);
        curve.first = 0;
        curve.last = 2 * std::numbers::pi;
        curve.periodic = true;
        curve.scale = scaleOf(circle.centre) + circle.radius;
    }
    void operator()(const Arc2d& arc) const {
        Handle(Geom2d_Curve) basis =
            new Geom2d_Circle(gp_Ax2d(arc.centre, gp_Dir2d(1, 0)), arc.radius);
        // Without periodic adjustment: the source domain stays, also across the seam.
        handle = new Geom2d_TrimmedCurve(basis, arc.start, arc.end, true, false);
        curve.first = arc.start;
        curve.last = arc.end;
        curve.scale = scaleOf(arc.centre) + arc.radius;
    }
    void operator()(const Ellipse2d& ellipse) const {
        handle = new Geom2d_Ellipse(gp_Ax2d(ellipse.centre, gp_Dir2d(std::cos(ellipse.rotation),
                                                                     std::sin(ellipse.rotation))),
                                    ellipse.majorRadius, ellipse.minorRadius);
        curve.first = 0;
        curve.last = 2 * std::numbers::pi;
        curve.periodic = true;
        curve.scale = scaleOf(ellipse.centre) + ellipse.majorRadius;
    }
    void operator()(const BSpline2d& spline) const {
        handle = bsplineCurve(spline);
        curve.first = 0;
        curve.last = bsplineLastParameter(spline.poles.size());
        double extent = 0;
        for (const auto& pole : spline.poles)
            extent = std::max(extent, scaleOf(pole));
        curve.scale = extent;
    }
};
} // namespace

PlanarCurve::PlanarCurve(const CurveDefinition2d& definition) {
    detail::guarded(operation, [&] {
        auto impl = std::make_shared<Impl>();
        std::visit(Builder{*this, impl->curve}, definition);
        impl_ = std::move(impl);
    });
}

gp_Pnt2d PlanarCurve::value(double parameter) const {
    return detail::guarded(operation, [&] { return impl_->curve->Value(parameter); });
}

void PlanarCurve::d1(double parameter, gp_Pnt2d& point, gp_Vec2d& derivative) const {
    detail::guarded(operation, [&] { impl_->curve->D1(parameter, point, derivative); });
}

double PlanarCurve::parameterTolerance(double parameter, double distance) const {
    gp_Pnt2d point;
    gp_Vec2d derivative;
    d1(parameter, point, derivative);
    return distance / derivative.Magnitude();
}

double PlanarCurve::domainParameter(double parameter, double distance) const {
    double u = parameter;
    const auto& c = impl_->curve;
    if (c->IsPeriodic() || !Handle(Geom2d_TrimmedCurve)::DownCast(c).IsNull()) {
        // Only conics have angular domains. A trimmed line is not periodic.
        auto basis = c;
        if (auto trimmed = Handle(Geom2d_TrimmedCurve)::DownCast(basis); !trimmed.IsNull())
            basis = trimmed->BasisCurve();
        if (basis->IsPeriodic()) {
            const double period = basis->Period();
            u += std::ceil((first - u) / period) * period;
            const double eps = parameterTolerance(u, distance);
            if (u > last + eps && u - period >= first - eps)
                u -= period;
            if (periodic && std::abs(u - last) <= eps)
                u = first;
        }
    }
    const double eps = parameterTolerance(u, distance);
    if (std::abs(u - first) <= eps)
        return first;
    if (std::abs(u - last) <= eps)
        return last;
    if (u < first || u > last)
        throw KernelError(operation, "Intersection outside source domain");
    return u;
}

TopoDS_Edge PlanarCurve::edgeInPlane(const gp_Pln& plane, double from, double to) const {
    return detail::guarded(operation, [&] {
        return BRepBuilderAPI_MakeEdge(GeomAPI::To3d(impl_->curve, plane), from, to).Edge();
    });
}

LoopFace planarLoopFace(std::span<const LoopPiece> loop) {
    return detail::guarded(operation, [&] {
        const gp_Pln plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        BRepBuilderAPI_MakeWire wire;
        for (const auto& piece : loop) {
            auto edge = piece.curve->edgeInPlane(plane, piece.from, piece.to);
            if (piece.reversed)
                edge.Reverse();
            BRep_Builder builder;
            for (TopExp_Explorer vertex(edge, TopAbs_VERTEX); vertex.More(); vertex.Next())
                builder.UpdateVertex(TopoDS::Vertex(vertex.Current()), piece.vertexTolerance);
            wire.Add(edge);
            if (!wire.IsDone())
                throw KernelError(operation, "Region wire failed");
        }
        BRepBuilderAPI_MakeFace face(plane, wire.Wire(), true);
        if (!face.IsDone() || !BRepCheck_Analyzer(face.Face()).IsValid())
            throw KernelError(operation, "Region classifier face invalid");
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face.Face(), properties);
        return LoopFace{face.Face(), properties.Mass()};
    });
}

CurveIntersection intersectCurves(const PlanarCurve& first, const PlanarCurve& second,
                                  double tolerance) {
    return detail::guarded(operation, [&] {
        CurveIntersection result;
        Geom2dAPI_InterCurveCurve intersection(first.impl().curve, second.impl().curve, tolerance);
        result.done = intersection.Intersector().IsDone();
        if (!result.done)
            return result;
        result.segments = intersection.NbSegments() > 0;
        for (int index = 1; index <= intersection.NbPoints(); ++index) {
            const auto& hit = intersection.Intersector().Point(index);
            result.points.push_back({hit.ParamOnFirst(), hit.ParamOnSecond()});
        }
        return result;
    });
}

} // namespace voelund::kernel
