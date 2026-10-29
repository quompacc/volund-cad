// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <variant>
#include <vector>

// K-GRENZE (2026-10-22): the curves of a sketch in its plane, with their parameter domains, for
// the sketch evaluation above the kernel. A failing OCCT computation is reported as KernelError.
namespace voelund::kernel {

// A segment from start to end, parameterised by arc length from the start.
struct Line2d final {
    gp_Pnt2d start;
    gp_Pnt2d end;
};
// A full circle, angle from the x axis.
struct Circle2d final {
    gp_Pnt2d centre;
    double radius;
};
// A counter-clockwise arc from `start` to `end` (radians, the source domain kept as given).
struct Arc2d final {
    gp_Pnt2d centre;
    double radius;
    double start;
    double end;
};
// A full ellipse whose major axis is turned by `rotation` (radians).
struct Ellipse2d final {
    gp_Pnt2d centre;
    double majorRadius;
    double minorRadius;
    double rotation;
};
// S21: an open, clamped, uniform B-spline over its poles: cubic (lower for fewer poles), knots
// 0, 1, ..., n - degree.
// N14c2: `knots` flat and clamped (each repeated by its multiplicity, poles + degree + 1 of
// them); none means the uniform knots above.
struct BSpline2d final {
    std::vector<gp_Pnt2d> poles;
    std::vector<double> knots;
};
// N14c: a piece of an ellipse, counter-clockwise between two eccentric anomalies (radians, the
// source domain kept as given).
struct EllipticArc2d final {
    Ellipse2d ellipse;
    double start;
    double end;
};
using CurveDefinition2d =
    std::variant<Line2d, Circle2d, Arc2d, Ellipse2d, BSpline2d, EllipticArc2d>;

[[nodiscard]] inline int bsplineDegree(std::size_t poles) noexcept {
    return std::min(3, static_cast<int>(poles) - 1);
}
[[nodiscard]] inline double bsplineLastParameter(std::size_t poles) noexcept {
    return static_cast<double>(poles) - bsplineDegree(poles);
}

// N14c2: the exact piece of a B-spline between two of its parameters (from < to), with its
// knots moved to start at 0.
[[nodiscard]] BSpline2d bsplinePiece(const BSpline2d& spline, double from, double to);

class PlanarCurve final {
  public:
    explicit PlanarCurve(const CurveDefinition2d& definition);

    double first{0};
    double last{0};
    bool periodic{false};
    double scale{0}; // size of the curve's coordinates, for tolerances

    [[nodiscard]] gp_Pnt2d value(double parameter) const;
    // N14c2: the parameter in [first, last] of the curve point nearest `point`.
    [[nodiscard]] double nearestParameter(const gp_Pnt2d& point) const;
    void d1(double parameter, gp_Pnt2d& point, gp_Vec2d& derivative) const;
    // The parameter distance that corresponds to `distance` along the curve at `parameter`.
    [[nodiscard]] double parameterTolerance(double parameter, double distance) const;
    // `parameter` brought into [first, last] (periods, snapping to the ends within the distance
    // tolerance); KernelError when it lies outside.
    [[nodiscard]] double domainParameter(double parameter, double distance) const;
    // The edge over [from, to] on this curve lifted into `plane` (the curve's x/y are the plane's).
    [[nodiscard]] TopoDS_Edge edgeInPlane(const gp_Pln& plane, double from, double to) const;

    struct Impl;
    [[nodiscard]] const Impl& impl() const noexcept { return *impl_; }

  private:
    std::shared_ptr<const Impl> impl_;
};

struct CurveHit final {
    double first;
    double second;
};
struct CurveIntersection final {
    bool done{false};
    bool segments{false}; // a stretch where the curves coincide within the tolerance
    std::vector<CurveHit> points;
};
// Where two curves meet within `tolerance` (parameters on the unbounded basis curves).
[[nodiscard]] CurveIntersection intersectCurves(const PlanarCurve& first, const PlanarCurve& second,
                                                double tolerance);

// A stretch of a curve in a closed loop on the XY plane; its vertices take `vertexTolerance`.
struct LoopPiece final {
    const PlanarCurve* curve;
    double from;
    double to;
    bool reversed;
    double vertexTolerance;
};
struct LoopFace final {
    TopoDS_Face face;
    double area;
};
// The face on the XY plane (z = 0) inside the loop, checked for validity; KernelError when the
// loop does not close or the face is invalid.
[[nodiscard]] LoopFace planarLoopFace(std::span<const LoopPiece> loop);

} // namespace voelund::kernel
