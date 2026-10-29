// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchContourGeometry.hpp"
#include "SketchConnection.hpp"
#include "SketchIntersectionCurves.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace voelund::features::detail {
namespace {
using namespace document;
constexpr double tau = 2 * std::numbers::pi;
SketchPoint radial(SketchPoint c, double r, double a) {
    return {c.x + r * std::cos(a), c.y + r * std::sin(a)};
}
double cross(SketchPoint a, SketchPoint b) { return a.x * b.y - a.y * b.x; }
SketchPoint sub(SketchPoint a, SketchPoint b) { return {a.x - b.x, a.y - b.y}; }
bool lineOverlap(const SketchLine& a, const SketchLine& b) {
    auto v = sub(a.end, a.start);
    const double length = std::hypot(v.x, v.y);
    const double eps = std::max(connectionTolerance(a.start, b.start).millimetres,
                                connectionTolerance(a.end, b.end).millimetres);
    if (std::abs(cross(v, sub(b.start, a.start))) > eps * length ||
        std::abs(cross(v, sub(b.end, a.start))) > eps * length)
        return false;
    auto project = [&](SketchPoint p) {
        auto d = sub(p, a.start);
        return (d.x * v.x + d.y * v.y) / length;
    };
    double first = project(b.start), last = project(b.end);
    if (first > last)
        std::swap(first, last);
    return std::min(length, last) - std::max(0., first) > eps;
}
struct Round {
    SketchPoint c;
    double r;
    double start;
    double sweep;
};
std::optional<Round> round(const SketchGeometry& g) {
    if (auto* c = std::get_if<SketchCircle>(&g))
        return Round{c->center, c->radius, 0, tau};
    if (auto* c = std::get_if<SketchArc>(&g))
        return Round{c->center, c->radius, c->startAngle, c->endAngle - c->startAngle};
    if (auto* c = std::get_if<SketchEllipse>(&g); c && c->majorRadius == c->minorRadius)
        return Round{c->center, c->majorRadius, 0, tau};
    return std::nullopt;
}
// N14c: an ellipse or a piece of one, with its parameter range.
struct Oval {
    SketchEllipse ellipse;
    double start;
    double sweep;
};
std::optional<Oval> oval(const SketchGeometry& g) {
    if (auto* e = std::get_if<SketchEllipse>(&g))
        return Oval{*e, 0, tau};
    if (auto* e = std::get_if<document::SketchEllipticArc>(&g))
        return Oval{e->ellipse(), e->startParameter, e->endParameter - e->startParameter};
    return std::nullopt;
}
// The ends of an open curve: a line, arc, B-spline (S21: its end poles) or elliptic arc.
std::optional<std::pair<SketchPoint, SketchPoint>> openEnds(const SketchGeometry& g) {
    if (std::holds_alternative<SketchLine>(g) || std::holds_alternative<SketchArc>(g) ||
        std::holds_alternative<document::SketchBSpline>(g) ||
        std::holds_alternative<document::SketchEllipticArc>(g))
        return std::pair{*document::sketchReferencePoint(g, document::SketchPointPosition::start),
                         *document::sketchReferencePoint(g, document::SketchPointPosition::end)};
    return std::nullopt;
}
double angle(double a) {
    a = std::fmod(a, tau);
    return a < 0 ? a + tau : a;
}
} // namespace
std::vector<ContourPiece> contourPieces(std::span<const SketchElement> elements) {
    std::vector<ContourPiece> result;
    for (const auto& e : elements) {
        if (e.construction)
            continue;
        auto add = [&](SketchGeometry g, SketchPoint a, SketchPoint b, unsigned part = 0,
                       bool periodic = false) {
            result.push_back({e.id, part, std::move(g), a, b, periodic});
        };
        if (const auto ends = openEnds(e.geometry)) // lines, arcs, B-splines, elliptic arcs
            add(e.geometry, ends->first, ends->second);
        else if (auto* r = std::get_if<SketchRectangle>(&e.geometry)) {
            const SketchPoint p[] = {
                r->first, {r->second.x, r->first.y}, r->second, {r->first.x, r->second.y}};
            for (unsigned i = 0; i < 4; ++i)
                add(SketchLine{p[i], p[(i + 1) % 4]}, p[i], p[(i + 1) % 4], i);
        } else if (auto* c = std::get_if<SketchCircle>(&e.geometry)) {
            const auto p = radial(c->center, c->radius, 0);
            add(*c, p, p, 0, true);
        } else if (std::holds_alternative<document::SketchPointGeometry>(e.geometry)) {
            continue; // construction anyway
        } else {
            const auto& ellipse = std::get<SketchEllipse>(e.geometry);
            const auto p = radial(ellipse.center, ellipse.majorRadius, ellipse.rotation);
            add(ellipse, p, p, 0, true);
        }
    }
    return result;
}
SketchPoint contourMiddle(const ContourPiece& p) {
    if (auto* a = std::get_if<SketchArc>(&p.geometry))
        return radial(a->center, a->radius, (a->startAngle + a->endAngle) / 2);
    if (auto* s = std::get_if<document::SketchBSpline>(&p.geometry)) {
        const auto knots = document::bsplineKnots(*s); // N14c2
        const auto middle = planarCurve(p).value((knots.front() + knots.back()) / 2);
        return {middle.X(), middle.Y()};
    }
    if (auto* e = std::get_if<document::SketchEllipticArc>(&p.geometry))
        return document::ellipsePoint(e->ellipse(), (e->startParameter + e->endParameter) / 2);
    return {(p.start.x + p.end.x) / 2, (p.start.y + p.end.y) / 2};
}
bool contourInterior(const ContourPiece& p, SketchPoint q) {
    if (connected(p.start, q) || connected(p.end, q))
        return false;
    if (auto* l = std::get_if<SketchLine>(&p.geometry)) {
        auto v = sub(l->end, l->start), d = sub(q, l->start);
        const double length = std::hypot(v.x, v.y);
        const double eps = connectionTolerance(q, l->start).millimetres;
        const double t = (d.x * v.x + d.y * v.y) / (length * length);
        return t > 0 && t < 1 && std::abs(cross(v, d)) <= eps * length;
    }
    if (auto* a = std::get_if<SketchArc>(&p.geometry)) {
        auto d = sub(q, a->center);
        return std::abs(std::hypot(d.x, d.y) - a->radius) <=
                   connectionTolerance(q, a->center).millimetres &&
               angle(std::atan2(d.y, d.x) - a->startAngle) < a->endAngle - a->startAngle;
    }
    return false;
}
namespace {
// N14c: two ellipses or pieces of one overlap on the same ellipse.
bool ovalOverlap(const ContourPiece& a, const ContourPiece& b) {
    const auto oa = oval(a.geometry), ob = oval(b.geometry);
    if (!oa || !ob)
        return false;
    const auto &ea = oa->ellipse, &eb = ob->ellipse;
    const double eps = connectionTolerance(ea.center, eb.center).millimetres;
    const double turn = std::remainder(eb.rotation - ea.rotation, std::numbers::pi);
    if (!connected(ea.center, eb.center) || std::abs(ea.majorRadius - eb.majorRadius) > eps ||
        std::abs(ea.minorRadius - eb.minorRadius) > eps ||
        std::abs(turn) > kernel::angularTolerance.radians)
        return false;
    // The parameters of b on a's ellipse: shifted by pi where its major axis points back.
    const double shift = std::remainder(eb.rotation - ea.rotation, tau) - turn;
    const double start = angle(oa->start), other = angle(ob->start + shift);
    for (int step = -1; step <= 1; ++step) {
        const double t = other + step * tau;
        if (std::min(start + oa->sweep, t + ob->sweep) - std::max(start, t) >
            kernel::angularTolerance.radians)
            return true;
    }
    return false;
}
} // namespace
bool contourOverlap(const ContourPiece& a, const ContourPiece& b) {
    if (auto* l = std::get_if<SketchLine>(&a.geometry)) {
        auto* other = std::get_if<SketchLine>(&b.geometry);
        return other && lineOverlap(*l, *other);
    }
    const auto ra = round(a.geometry), rb = round(b.geometry);
    if (ra && rb) {
        const double eps = connectionTolerance(ra->c, rb->c).millimetres;
        if (!connected(ra->c, rb->c) || std::abs(ra->r - rb->r) > eps)
            return false;
        const double start = angle(ra->start), other = angle(rb->start);
        for (int shift = -1; shift <= 1; ++shift) {
            const double t = other + shift * tau;
            if (std::min(start + ra->sweep, t + rb->sweep) - std::max(start, t) >
                kernel::angularTolerance.radians)
                return true;
        }
        return false;
    }
    return ovalOverlap(a, b);
}
} // namespace voelund::features::detail
