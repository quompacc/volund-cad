// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchCurveCornerPlan.hpp"

#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace voelund::application::detail {
namespace {

using document::SketchArc;
using document::SketchGeometry;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double tolerance = kernel::minimumModelLength;
constexpr double turn = 2.0 * std::numbers::pi;

double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
Position other(Position position) {
    return position == Position::start ? Position::end : Position::start;
}
SketchPoint onArc(const SketchArc& arc, double angle) {
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}
double angleOf(SketchPoint centre, SketchPoint point) {
    return std::atan2(point.y - centre.y, point.x - centre.x);
}
SketchPoint endOf(const SketchGeometry& geometry, Position end) {
    if (const auto* line = std::get_if<SketchLine>(&geometry))
        return end == Position::start ? line->start : line->end;
    const auto& arc = std::get<SketchArc>(geometry);
    return onArc(arc, end == Position::start ? arc.startAngle : arc.endAngle);
}

struct Corner final {
    Position first;
    Position second;
    SketchPoint point;
};
std::optional<Corner> cornerOf(const SketchGeometry& first, const SketchGeometry& second) {
    for (const auto a : {Position::start, Position::end})
        for (const auto b : {Position::start, Position::end})
            if (distance(endOf(first, a), endOf(second, b)) < tolerance)
                return Corner{a, b, endOf(first, a)};
    return std::nullopt;
}

// A curve as an infinite line (point and unit direction) or a full circle, for offsets.
struct Primitive final {
    bool line;
    SketchPoint origin; // a point of the line, or the circle's centre
    SketchPoint direction;
    double radius;
};
Primitive primitiveOf(const SketchGeometry& geometry) {
    if (const auto* line = std::get_if<SketchLine>(&geometry)) {
        const double length = distance(line->start, line->end);
        return {true,
                line->start,
                {(line->end.x - line->start.x) / length, (line->end.y - line->start.y) / length},
                0.0};
    }
    const auto& arc = std::get<SketchArc>(geometry);
    return {false, arc.center, {}, arc.radius};
}
std::optional<Primitive> offset(Primitive primitive, double by) {
    if (primitive.line) {
        primitive.origin = {primitive.origin.x - primitive.direction.y * by,
                            primitive.origin.y + primitive.direction.x * by};
        return primitive;
    }
    primitive.radius += by;
    return primitive.radius > tolerance ? std::optional{primitive} : std::nullopt;
}

std::vector<SketchPoint> lineCircle(const Primitive& line, const Primitive& circle) {
    const double fx = line.origin.x - circle.origin.x, fy = line.origin.y - circle.origin.y;
    const double b = fx * line.direction.x + fy * line.direction.y;
    const double c = fx * fx + fy * fy - circle.radius * circle.radius;
    const double disc = b * b - c;
    if (disc < 0.0)
        return {};
    std::vector<SketchPoint> result;
    for (const double t : {-b - std::sqrt(disc), -b + std::sqrt(disc)})
        result.push_back(
            {line.origin.x + line.direction.x * t, line.origin.y + line.direction.y * t});
    return result;
}
std::vector<SketchPoint> circleCircle(const Primitive& a, const Primitive& b) {
    const double d = distance(a.origin, b.origin);
    if (d < 1e-12 || d > a.radius + b.radius || d < std::abs(a.radius - b.radius))
        return {};
    const double along = (a.radius * a.radius - b.radius * b.radius + d * d) / (2 * d);
    const double h = std::sqrt(std::max(0.0, a.radius * a.radius - along * along));
    const SketchPoint u{(b.origin.x - a.origin.x) / d, (b.origin.y - a.origin.y) / d};
    const SketchPoint base{a.origin.x + u.x * along, a.origin.y + u.y * along};
    return {{base.x - u.y * h, base.y + u.x * h}, {base.x + u.y * h, base.y - u.x * h}};
}
std::vector<SketchPoint> lineLine(const Primitive& a, const Primitive& b) {
    const double cross = a.direction.x * b.direction.y - a.direction.y * b.direction.x;
    if (std::abs(cross) < 1e-12)
        return {};
    const double t =
        ((b.origin.x - a.origin.x) * b.direction.y - (b.origin.y - a.origin.y) * b.direction.x) /
        cross;
    return {{a.origin.x + a.direction.x * t, a.origin.y + a.direction.y * t}};
}
std::vector<SketchPoint> intersections(const Primitive& a, const Primitive& b) {
    if (a.line && b.line)
        return lineLine(a, b);
    if (a.line || b.line)
        return a.line ? lineCircle(a, b) : lineCircle(b, a);
    return circleCircle(a, b);
}

SketchPoint foot(const Primitive& primitive, SketchPoint point) {
    if (primitive.line) {
        const double t = (point.x - primitive.origin.x) * primitive.direction.x +
                         (point.y - primitive.origin.y) * primitive.direction.y;
        return {primitive.origin.x + primitive.direction.x * t,
                primitive.origin.y + primitive.direction.y * t};
    }
    const double d = distance(primitive.origin, point);
    return {primitive.origin.x + (point.x - primitive.origin.x) / d * primitive.radius,
            primitive.origin.y + (point.y - primitive.origin.y) / d * primitive.radius};
}

// The point lies on the part of the curve between the corner (excluded) and its other end.
bool onKeptPart(const SketchGeometry& geometry, Position corner, SketchPoint point) {
    if (const auto* line = std::get_if<SketchLine>(&geometry)) {
        const SketchPoint from = endOf(geometry, corner), to = endOf(geometry, other(corner));
        const double length = distance(from, to);
        const double t =
            ((point.x - from.x) * (to.x - from.x) + (point.y - from.y) * (to.y - from.y)) /
            (length * length);
        return t > tolerance / length && t < 1.0 - tolerance / length && line != nullptr;
    }
    const auto& arc = std::get<SketchArc>(geometry);
    const double sweep = arc.endAngle - arc.startAngle;
    const double relative = std::fmod(angleOf(arc.center, point) - arc.startAngle + 2 * turn, turn);
    const double margin = tolerance / arc.radius;
    return relative > margin && relative < sweep - margin;
}

// The curve cut back so that its corner end lies at `joint` (a point of the curve).
SketchGeometry cutBack(SketchGeometry geometry, Position corner, SketchPoint joint) {
    if (auto* line = std::get_if<SketchLine>(&geometry)) {
        (corner == Position::start ? line->start : line->end) = joint;
        return geometry;
    }
    auto& arc = std::get<SketchArc>(geometry);
    double angle = angleOf(arc.center, joint);
    if (corner == Position::start) {
        while (angle >= arc.endAngle)
            angle -= turn;
        while (angle < arc.endAngle - turn)
            angle += turn;
        arc.startAngle = angle;
    } else {
        while (angle <= arc.startAngle)
            angle += turn;
        while (angle > arc.startAngle + turn)
            angle -= turn;
        arc.endAngle = angle;
    }
    return geometry;
}

struct Joints final {
    SketchPoint first;
    SketchPoint second;
    SketchPoint centre;
};

// The fillet circle's centre `radius` from both curves, touching each on its kept part, the
// touches nearest the corner.
std::optional<Joints> filletJoints(const CornerCurve& first, const CornerCurve& second,
                                   const Corner& corner, double radius) {
    const auto a = primitiveOf(first.geometry), b = primitiveOf(second.geometry);
    std::optional<Joints> best;
    double bestScore = 0.0;
    for (const double sa : {-1.0, 1.0})
        for (const double sb : {-1.0, 1.0}) {
            const auto oa = offset(a, sa * radius), ob = offset(b, sb * radius);
            if (!oa || !ob)
                continue;
            for (const auto centre : intersections(*oa, *ob)) {
                const auto fa = foot(a, centre), fb = foot(b, centre);
                if (!onKeptPart(first.geometry, corner.first, fa) ||
                    !onKeptPart(second.geometry, corner.second, fb))
                    continue;
                const double score = distance(fa, corner.point) + distance(fb, corner.point);
                if (!best || score < bestScore) {
                    best = Joints{fa, fb, centre};
                    bestScore = score;
                }
            }
        }
    return best;
}

// The point of the curve `size` (straight) from the corner towards its other end.
std::optional<SketchPoint> chamferJoint(const SketchGeometry& geometry, Position corner,
                                        double size) {
    if (std::holds_alternative<SketchLine>(geometry)) {
        const SketchPoint from = endOf(geometry, corner), to = endOf(geometry, other(corner));
        const double length = distance(from, to);
        if (size >= length - tolerance)
            return std::nullopt;
        return SketchPoint{from.x + (to.x - from.x) / length * size,
                           from.y + (to.y - from.y) / length * size};
    }
    const auto& arc = std::get<SketchArc>(geometry);
    if (size >= 2 * arc.radius)
        return std::nullopt;
    const double step = 2 * std::asin(size / (2 * arc.radius));
    if (step >= arc.endAngle - arc.startAngle - tolerance / arc.radius)
        return std::nullopt;
    return onArc(arc, corner == Position::start ? arc.startAngle + step : arc.endAngle - step);
}

} // namespace

CurveCornerPlan planCurveCorner(SketchCornerKind kind, const CornerCurve& first,
                                const CornerCurve& second, double size) {
    const bool chamfer = kind == SketchCornerKind::chamfer;
    const auto refuse = [&](const std::string& reason) {
        throw SketchEditRefusal((chamfer ? "Fasen: " : "Verrunden: ") + reason,
                                {first.id, second.id});
    };
    if (!std::isfinite(size) || size <= tolerance)
        refuse(chamfer ? "Der Abstand muss größer als 0 sein"
                       : "Der Radius muss größer als 0 sein");
    const auto corner = cornerOf(first.geometry, second.geometry);
    if (!corner)
        refuse("Linie und Bogen oder zwei Bögen müssen sich an einem Endpunkt treffen");
    CurveCornerPlan plan{{first.id, first.geometry, corner->first},
                         {second.id, second.geometry, corner->second},
                         {},
                         true,
                         {}};
    if (chamfer) {
        const auto a = chamferJoint(first.geometry, corner->first, size);
        const auto b = chamferJoint(second.geometry, corner->second, size);
        if (!a || !b)
            refuse("Der Abstand ist zu groß für die Kurven");
        plan.first.geometry = cutBack(first.geometry, corner->first, *a);
        plan.second.geometry = cutBack(second.geometry, corner->second, *b);
        plan.chamfer = SketchLine{*a, *b};
        return plan;
    }
    const auto joints = filletJoints(first, second, *corner, size);
    if (!joints)
        refuse("Der Radius passt nicht in die Ecke");
    plan.first.geometry = cutBack(first.geometry, corner->first, joints->first);
    plan.second.geometry = cutBack(second.geometry, corner->second, joints->second);
    double start = angleOf(joints->centre, joints->first);
    double end = angleOf(joints->centre, joints->second);
    double sweep = std::fmod(end - start + turn, turn);
    if (sweep > std::numbers::pi) {
        std::swap(start, end);
        sweep = turn - sweep;
        plan.arcStartOnFirst = false;
    }
    plan.arc = SketchArc{joints->centre, size, start, start + sweep};
    return plan;
}

} // namespace voelund::application::detail
