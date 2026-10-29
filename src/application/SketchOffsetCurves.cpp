// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchOffsetCurves.hpp"
#include "SketchCurveEditCuts.hpp"
#include "SketchSplineFit.hpp"

#include <voelund/kernel/PlanarCurve.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace voelund::application::detail {
namespace {

using document::SketchPoint;
using Position = document::SketchPointPosition;

[[noreturn]] void refuse(const std::string& reason) {
    throw std::invalid_argument("Versatz: " + reason);
}

kernel::Ellipse2d ellipse2d(const document::SketchEllipse& e) {
    return {{e.center.x, e.center.y}, e.majorRadius, e.minorRadius, e.rotation};
}

// The curve point at `t` moved by `left` along its left normal.
SketchPoint offsetPoint(const kernel::PlanarCurve& curve, double t, double left) {
    gp_Pnt2d point;
    gp_Vec2d tangent;
    curve.d1(t, point, tangent);
    const double length = tangent.Magnitude();
    return {point.X() - tangent.Y() / length * left, point.Y() + tangent.X() / length * left};
}

// The derivative of the offset curve at `t` for a parameter step `step` per spline unit,
// by central differences (one-sided at the curve's ends).
SketchPoint offsetDerivative(const kernel::PlanarCurve& curve, double t, double left, double step) {
    const double h = 1e-5 * step;
    const double a = std::max(curve.first, t - h), b = std::min(curve.last, t + h);
    const auto p = offsetPoint(curve, a, left), q = offsetPoint(curve, b, left);
    return {(q.x - p.x) / (b - a) * step, (q.y - p.y) / (b - a) * step};
}

// The offset turns back where the distance exceeds the radius of curvature: neighbouring
// offset points then step against the curve's own direction.
void requireNoReversal(const kernel::PlanarCurve& curve, double first, double last, double left) {
    constexpr int samples = 256;
    for (int index = 0; index < samples; ++index) {
        const double a = first + (last - first) * index / samples;
        const double b = first + (last - first) * (index + 1) / samples;
        const auto p = curve.value(a), q = curve.value(b);
        const auto u = offsetPoint(curve, a, left), v = offsetPoint(curve, b, left);
        if ((v.x - u.x) * (q.X() - p.X()) + (v.y - u.y) * (q.Y() - p.Y()) <= 0.0)
            refuse("Der Abstand ist größer als die Krümmung erlaubt; der Versatz würde sich "
                   "selbst schneiden");
    }
}

// How far the spline strays from the distance between its fit points.
double fitError(const kernel::PlanarCurve& curve, const std::vector<SketchPoint>& poles,
                std::size_t points, double left) {
    double worst = 0.0;
    for (std::size_t index = 0; index + 1 < points; ++index)
        for (const double share : {0.25, 0.5, 0.75}) {
            const auto at = splinePoint(poles, static_cast<double>(index) + share);
            const auto foot = curve.value(curve.nearestParameter({at.x, at.y}));
            worst = std::max(
                worst, std::abs(std::hypot(at.x - foot.X(), at.y - foot.Y()) - std::abs(left)));
        }
    return worst;
}

// The spline through offset points of [first, last], fine enough for the tolerance.
document::SketchBSpline offsetSpline(const kernel::PlanarCurve& curve, double first, double last,
                                     double left) {
    requireNoReversal(curve, first, last, left);
    for (const std::size_t points : {8U, 16U, 32U, 62U}) {
        std::vector<SketchPoint> fit;
        for (std::size_t index = 0; index < points; ++index)
            fit.push_back(offsetPoint(curve,
                                      first + (last - first) * static_cast<double>(index) /
                                                  static_cast<double>(points - 1),
                                      left));
        // The offset's own end tangents, by the spline parameter (one step per fit point).
        const double step = (last - first) / static_cast<double>(points - 1);
        auto poles = splineThroughPoints(fit, offsetDerivative(curve, first, left, step),
                                         offsetDerivative(curve, last, left, step));
        if (poles.empty())
            refuse("Der Versatz fällt zu einem Punkt zusammen");
        if (fitError(curve, poles, points, left) <= offsetCurveTolerance)
            return {std::move(poles), {}};
    }
    refuse("Die Kurve ist zu stark gekrümmt, um ihren Versatz genau als Spline darzustellen");
}

} // namespace

bool offsetsAsSpline(const document::SketchGeometry& geometry) noexcept {
    return std::holds_alternative<document::SketchEllipse>(geometry) ||
           std::holds_alternative<document::SketchEllipticArc>(geometry) ||
           std::holds_alternative<document::SketchBSpline>(geometry);
}

SketchOffsetResult offsetAsSpline(const document::SketchGeometry& geometry, double distance) {
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry)) {
        // Counter-clockwise, its left side is inside: outwards is to the right.
        const kernel::PlanarCurve curve(ellipse2d(*ellipse));
        constexpr double pi = std::numbers::pi;
        return {
            {offsetSpline(curve, 0.0, pi, -distance), offsetSpline(curve, pi, 2.0 * pi, -distance)},
            {{0, Position::end, 1, Position::start}, {1, Position::end, 0, Position::start}}};
    }
    if (const auto* arc = std::get_if<document::SketchEllipticArc>(&geometry)) {
        const kernel::PlanarCurve curve(kernel::EllipticArc2d{
            ellipse2d(arc->ellipse()), arc->startParameter, arc->endParameter});
        return {{offsetSpline(curve, curve.first, curve.last, distance)}, {}};
    }
    const kernel::PlanarCurve curve(planarSpline(std::get<document::SketchBSpline>(geometry)));
    return {{offsetSpline(curve, curve.first, curve.last, distance)}, {}};
}

} // namespace voelund::application::detail
