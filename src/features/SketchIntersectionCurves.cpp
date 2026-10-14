#include "SketchIntersectionCurves.hpp"
#include "SketchBSplineCurves.hpp"
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax2d.hxx>
#include <gp_Vec2d.hxx>
#include <numbers>
#include <stdexcept>
namespace voelund::features::detail {
namespace {
double scale(document::SketchPoint p) { return std::max(std::abs(p.x), std::abs(p.y)); }
} // namespace
PlanarCurve planarCurve(const ContourPiece& piece) {
    using namespace document;
    constexpr double tau = 2 * std::numbers::pi;
    if (auto* l = std::get_if<SketchLine>(&piece.geometry)) {
        const gp_Pnt2d a(l->start.x, l->start.y), b(l->end.x, l->end.y);
        const double length = a.Distance(b);
        Handle(Geom2d_Curve) line = new Geom2d_Line(a, gp_Dir2d(gp_Vec2d(a, b)));
        return {new Geom2d_TrimmedCurve(line, 0, length), 0, length, false,
                std::max(scale(l->start), scale(l->end))};
    }
    if (auto* c = std::get_if<SketchCircle>(&piece.geometry)) {
        return {new Geom2d_Circle(gp_Ax2d({c->center.x, c->center.y}, {1, 0}), c->radius), 0, tau,
                true, scale(c->center) + c->radius};
    }
    if (auto* a = std::get_if<SketchArc>(&piece.geometry)) {
        Handle(Geom2d_Curve) circle =
            new Geom2d_Circle(gp_Ax2d({a->center.x, a->center.y}, {1, 0}), a->radius);
        // Disable periodic adjustment: preserve the source domain, including seam-crossing arcs.
        return {new Geom2d_TrimmedCurve(circle, a->startAngle, a->endAngle, true, false),
                a->startAngle, a->endAngle, false, scale(a->center) + a->radius};
    }
    if (auto* e = std::get_if<SketchEllipse>(&piece.geometry)) {
        return {new Geom2d_Ellipse(gp_Ax2d({e->center.x, e->center.y},
                                           {std::cos(e->rotation), std::sin(e->rotation)}),
                                   e->majorRadius, e->minorRadius),
                0, tau, true, scale(e->center) + e->majorRadius};
    }
    if (auto* s = std::get_if<SketchBSpline>(&piece.geometry)) { // S21
        double extent = 0;
        for (const auto& pole : s->poles)
            extent = std::max(extent, scale(pole));
        return {bsplineCurve2d(*s), 0, bsplineLastParameter(*s), false, extent};
    }
    throw std::logic_error("Rectangle must be expanded into contour pieces");
}
double parameterTolerance(const PlanarCurve& c, double u, double distance) {
    gp_Pnt2d p;
    gp_Vec2d d;
    c.curve->D1(u, p, d);
    return distance / d.Magnitude();
}
double domainParameter(const PlanarCurve& c, double u, double distance) {
    if (c.curve->IsPeriodic() || Handle(Geom2d_TrimmedCurve)::DownCast(c.curve).IsNull() == false) {
        // Only conics have angular domains. A trimmed line is not periodic.
        auto base = c.curve;
        if (auto trimmed = Handle(Geom2d_TrimmedCurve)::DownCast(base); !trimmed.IsNull())
            base = trimmed->BasisCurve();
        if (base->IsPeriodic()) {
            const double period = base->Period();
            u += std::ceil((c.first - u) / period) * period;
            const double eps = parameterTolerance(c, u, distance);
            if (u > c.last + eps && u - period >= c.first - eps)
                u -= period;
            if (c.periodic && std::abs(u - c.last) <= eps)
                u = c.first;
        }
    }
    const double eps = parameterTolerance(c, u, distance);
    if (std::abs(u - c.first) <= eps)
        return c.first;
    if (std::abs(u - c.last) <= eps)
        return c.last;
    if (u < c.first || u > c.last)
        throw std::runtime_error("Intersection outside source domain");
    return u;
}
} // namespace voelund::features::detail
