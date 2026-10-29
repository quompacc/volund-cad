// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchReferencePoints.hpp"

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace voelund::application::detail {
namespace {

using document::SketchPoint;
using Position = document::SketchPointPosition;
using Candidates = std::vector<std::pair<Position, SketchPoint>>;

SketchPoint onCircle(SketchPoint center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}

Candidates candidates(const document::SketchLine& line) {
    return {{Position::start, line.start}, {Position::end, line.end}};
}
Candidates candidates(const document::SketchBSpline& spline) {
    return {{Position::start, spline.poles.front()}, {Position::end, spline.poles.back()}};
}
Candidates candidates(const document::SketchCircle& circle) {
    return {{Position::center, circle.center}};
}
Candidates candidates(const document::SketchEllipse& ellipse) {
    return {{Position::center, ellipse.center}};
}
Candidates candidates(const document::SketchPointGeometry& point) {
    return {{Position::center, point.position}};
}
Candidates candidates(const document::SketchArc& arc) {
    return {{Position::center, arc.center},
            {Position::start, onCircle(arc.center, arc.radius, arc.startAngle)},
            {Position::end, onCircle(arc.center, arc.radius, arc.endAngle)}};
}
Candidates candidates(const document::SketchEllipticArc& arc) {
    return {{Position::center, arc.center},
            {Position::start, document::ellipsePoint(arc.ellipse(), arc.startParameter)},
            {Position::end, document::ellipsePoint(arc.ellipse(), arc.endParameter)}};
}
Candidates candidates(const document::SketchRectangle&) { return {}; }

void shift(SketchPoint& point, double dx, double dy) {
    point.x += dx;
    point.y += dy;
}

// The new parameter of a moved end, kept on its side of the other end (sweep in (0, 2 pi)).
double keptParameter(double parameter, Position end, double start, double finish) {
    constexpr double turn = 2.0 * std::numbers::pi;
    if (end == Position::end) {
        while (parameter <= start)
            parameter += turn;
        while (parameter > start + turn)
            parameter -= turn;
    } else {
        while (finish <= parameter)
            parameter -= turn;
        while (parameter < finish - turn)
            parameter += turn;
    }
    return parameter;
}

bool move(document::SketchLine& line, Position point, double dx, double dy) {
    shift(point == Position::start ? line.start : line.end, dx, dy);
    return true;
}
bool move(document::SketchBSpline& spline, Position point, double dx, double dy) {
    shift(point == Position::end ? spline.poles.back() : spline.poles.front(), dx, dy); // S21
    return true;
}
bool move(document::SketchCircle& circle, Position, double dx, double dy) {
    shift(circle.center, dx, dy);
    return true;
}
bool move(document::SketchEllipse& ellipse, Position, double dx, double dy) {
    shift(ellipse.center, dx, dy);
    return true;
}
bool move(document::SketchPointGeometry& point, Position, double dx, double dy) {
    shift(point.position, dx, dy); // S15
    return true;
}
bool move(document::SketchArc& arc, Position point, double dx, double dy) {
    if (point == Position::center) {
        shift(arc.center, dx, dy);
        return true;
    }
    double& angle = point == Position::start ? arc.startAngle : arc.endAngle;
    SketchPoint moved = onCircle(arc.center, arc.radius, angle);
    shift(moved, dx, dy);
    angle = keptParameter(std::atan2(moved.y - arc.center.y, moved.x - arc.center.x), point,
                          arc.startAngle, arc.endAngle);
    return true;
}
bool move(document::SketchEllipticArc& arc, Position point, double dx, double dy) {
    if (point == Position::center) {
        shift(arc.center, dx, dy);
        return true;
    }
    double& parameter = point == Position::start ? arc.startParameter : arc.endParameter;
    SketchPoint moved = document::ellipsePoint(arc.ellipse(), parameter);
    shift(moved, dx, dy);
    parameter = keptParameter(document::ellipseParameter(arc.ellipse(), moved), point,
                              arc.startParameter, arc.endParameter);
    return true;
}
bool move(document::SketchRectangle&, Position, double, double) { return false; }

} // namespace

std::optional<document::SketchPointPosition>
nearestReferencePoint(const document::SketchGeometry& geometry, document::SketchPoint position) {
    const auto points = std::visit([](const auto& value) { return candidates(value); }, geometry);
    std::optional<Position> nearest;
    double best = 0.0;
    for (const auto& [kind, point] : points) {
        const double distance = std::hypot(position.x - point.x, position.y - point.y);
        if (!nearest || distance < best) {
            nearest = kind;
            best = distance;
        }
    }
    return nearest;
}

bool moveReferencePoint(document::SketchGeometry& geometry, document::SketchPointPosition point,
                        double dx, double dy) {
    return std::visit([&](auto& value) { return move(value, point, dx, dy); }, geometry);
}

} // namespace voelund::application::detail
