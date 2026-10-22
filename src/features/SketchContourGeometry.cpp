// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchContourGeometry.hpp"
#include "SketchBSplineCurves.hpp"
#include "SketchConnection.hpp"
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
        if (auto* l = std::get_if<SketchLine>(&e.geometry))
            add(*l, l->start, l->end);
        else if (auto* a = std::get_if<SketchArc>(&e.geometry))
            add(*a, radial(a->center, a->radius, a->startAngle),
                radial(a->center, a->radius, a->endAngle));
        else if (auto* r = std::get_if<SketchRectangle>(&e.geometry)) {
            const SketchPoint p[] = {
                r->first, {r->second.x, r->first.y}, r->second, {r->first.x, r->second.y}};
            for (unsigned i = 0; i < 4; ++i)
                add(SketchLine{p[i], p[(i + 1) % 4]}, p[i], p[(i + 1) % 4], i);
        } else if (auto* c = std::get_if<SketchCircle>(&e.geometry)) {
            const auto p = radial(c->center, c->radius, 0);
            add(*c, p, p, 0, true);
        } else if (auto* s = std::get_if<document::SketchBSpline>(&e.geometry)) {
            add(*s, s->poles.front(), s->poles.back()); // S21: open, ends at its end poles
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
        const auto middle = bsplineCurve2d(*s)->Value(bsplineLastParameter(*s) / 2);
        return {middle.X(), middle.Y()};
    }
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
    auto* ea = std::get_if<SketchEllipse>(&a.geometry);
    auto* eb = std::get_if<SketchEllipse>(&b.geometry);
    if (!ea || !eb)
        return false;
    const double eps = connectionTolerance(ea->center, eb->center).millimetres;
    return connected(ea->center, eb->center) &&
           std::abs(ea->majorRadius - eb->majorRadius) <= eps &&
           std::abs(ea->minorRadius - eb->minorRadius) <= eps &&
           std::abs(std::remainder(ea->rotation - eb->rotation, std::numbers::pi)) <=
               kernel::angularTolerance.radians;
}
} // namespace voelund::features::detail
