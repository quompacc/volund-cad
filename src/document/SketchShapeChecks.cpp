// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchShapeChecks.hpp"

#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <variant>
#include <vector>

namespace voelund::document::detail {
namespace {

constexpr double minimumLength = 1.0e-9;

bool isFinite(double value) { return std::isfinite(value); }
bool finite(const SketchPoint& point) { return std::isfinite(point.x) && std::isfinite(point.y); }
double squaredDistance(const SketchPoint& first, const SketchPoint& second) {
    const double x = second.x - first.x;
    const double y = second.y - first.y;
    return x * x + y * y;
}
bool inModel(const SketchPoint& p) {
    return kernel::modelCoordinate(p.x) && kernel::modelCoordinate(p.y);
}
// A round shape's center and its box of `radius` stay in the model.
bool roundInModel(const SketchPoint& center, double radius) {
    return inModel(center) && kernel::modelLength(radius) &&
           inModel({center.x - radius, center.y - radius}) &&
           inModel({center.x + radius, center.y + radius});
}
bool sweepValid(double start, double end) {
    const double sweep = end - start;
    return std::isfinite(start) && std::isfinite(end) && sweep > kernel::angularTolerance.radians &&
           sweep < 2.0 * std::numbers::pi;
}
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}

// Within the model's coordinate and length range.
bool inRange(const SketchLine& s) {
    return inModel(s.start) && inModel(s.end) &&
           kernel::modelLength(std::hypot(s.end.x - s.start.x, s.end.y - s.start.y));
}
bool inRange(const SketchPointGeometry& s) { return inModel(s.position); }
bool inRange(const SketchBSpline& s) {
    double length = 0;
    for (std::size_t index = 1; index < s.poles.size(); ++index)
        length += std::hypot(s.poles[index].x - s.poles[index - 1].x,
                             s.poles[index].y - s.poles[index - 1].y);
    return std::ranges::all_of(s.poles, inModel) && kernel::modelLength(length);
}
bool inRange(const SketchRectangle& s) {
    return inModel(s.first) && inModel(s.second) &&
           kernel::modelLength(std::abs(s.second.x - s.first.x)) &&
           kernel::modelLength(std::abs(s.second.y - s.first.y));
}
bool inRange(const SketchCircle& s) { return roundInModel(s.center, s.radius); }
bool inRange(const SketchArc& s) {
    return roundInModel(s.center, s.radius) &&
           kernel::modelLength(s.radius * (s.endAngle - s.startAngle));
}
bool inRange(const SketchEllipse& s) {
    return roundInModel(s.center, s.majorRadius) && kernel::modelLength(s.minorRadius);
}
bool inRange(const SketchEllipticArc& s) { // N14c
    return inRange(s.ellipse()) &&
           kernel::modelLength(s.minorRadius * (s.endParameter - s.startParameter));
}

// N14c2: no stored knots, or poles + 4 finite ones, clamped (the first and last four equal),
// increasing, inner knots at most three times.
bool knotsValid(const SketchBSpline& spline) {
    const auto& knots = spline.knots;
    if (knots.empty())
        return true;
    if (knots.size() != spline.poles.size() + 4 || !std::ranges::all_of(knots, isFinite) ||
        !std::ranges::is_sorted(knots) || !(knots.front() < knots.back()))
        return false;
    std::vector<std::size_t> runs{1};
    for (std::size_t index = 1; index < knots.size(); ++index) {
        if (knots[index] == knots[index - 1])
            ++runs.back();
        else
            runs.push_back(1);
    }
    return runs.front() == 4 && runs.back() == 4 &&
           std::all_of(runs.begin() + 1, runs.end() - 1, [](std::size_t run) { return run <= 3; });
}

// Finite and not degenerate.
void check(const SketchLine& line) {
    require(finite(line.start) && finite(line.end) &&
                squaredDistance(line.start, line.end) > minimumLength * minimumLength,
            "Sketch line must have finite, distinct endpoints");
    require(line.constraint != SketchLineConstraint::horizontal ||
                std::abs(line.end.y - line.start.y) <= minimumLength,
            "Horizontal sketch line constraint is not satisfied");
    require(line.constraint != SketchLineConstraint::vertical ||
                std::abs(line.end.x - line.start.x) <= minimumLength,
            "Vertical sketch line constraint is not satisfied");
}
void check(const SketchRectangle& rectangle) {
    require(finite(rectangle.first) && finite(rectangle.second) &&
                std::abs(rectangle.second.x - rectangle.first.x) > minimumLength &&
                std::abs(rectangle.second.y - rectangle.first.y) > minimumLength,
            "Sketch rectangle must have a finite positive area");
}
void check(const SketchCircle& circle) {
    require(finite(circle.center) && std::isfinite(circle.radius) && circle.radius > minimumLength,
            "Sketch circle must have a finite positive radius");
}
void check(const SketchEllipse& ellipse) {
    require(finite(ellipse.center) && std::isfinite(ellipse.majorRadius) &&
                std::isfinite(ellipse.minorRadius) && std::isfinite(ellipse.rotation) &&
                ellipse.majorRadius > minimumLength && ellipse.minorRadius > minimumLength &&
                ellipse.minorRadius <= ellipse.majorRadius,
            "Sketch ellipse must have finite positive radii");
}
void check(const SketchPointGeometry& point) {
    require(finite(point.position), "Sketch point must be finite");
}
void check(const SketchBSpline& spline) { // S21
    const auto& poles = spline.poles;
    require(poles.size() >= minimumBSplinePoles && poles.size() <= maximumBSplinePoles &&
                std::ranges::all_of(poles, finite) &&
                squaredDistance(poles.front(), poles.back()) > minimumLength * minimumLength,
            "Sketch B-spline needs 4 to 64 finite control points and two distinct ends");
    require(knotsValid(spline), "Sketch B-spline knots must be clamped and increasing");
}
void check(const SketchEllipticArc& arc) { // N14c
    check(arc.ellipse());
    require(sweepValid(arc.startParameter, arc.endParameter),
            "Sketch elliptic arc must have a finite sweep");
}
void check(const SketchArc& arc) {
    require(finite(arc.center) && std::isfinite(arc.radius) && arc.radius > minimumLength &&
                sweepValid(arc.startAngle, arc.endAngle),
            "Sketch arc must have a finite center, radius and sweep");
}

} // namespace

void validateShape(const SketchGeometry& geometry) {
    std::visit(
        [](const auto& shape) {
            require(inRange(shape), kernel::modelRangeDiagnostic);
            check(shape);
        },
        geometry);
}

} // namespace voelund::document::detail

namespace voelund::document {
namespace {

using Position = SketchPointPosition;
std::optional<SketchPoint> pointOf(const SketchLine& s, Position p) {
    if (p == Position::center)
        return std::nullopt;
    return p == Position::start ? s.start : s.end;
}
std::optional<SketchPoint> pointOf(const SketchBSpline& s, Position p) { // S21: its two ends
    if (p == Position::center)
        return std::nullopt;
    return p == Position::start ? s.poles.front() : s.poles.back();
}
std::optional<SketchPoint> centerOnly(SketchPoint center, Position p) {
    return p == Position::center ? std::optional{center} : std::nullopt;
}
std::optional<SketchPoint> pointOf(const SketchCircle& s, Position p) {
    return centerOnly(s.center, p);
}
std::optional<SketchPoint> pointOf(const SketchEllipse& s, Position p) {
    return centerOnly(s.center, p);
}
std::optional<SketchPoint> pointOf(const SketchPointGeometry& s, Position p) {
    return centerOnly(s.position, p);
}
std::optional<SketchPoint> pointOf(const SketchArc& s, Position p) {
    if (p == Position::center)
        return s.center;
    const double angle = p == Position::start ? s.startAngle : s.endAngle;
    return SketchPoint{s.center.x + s.radius * std::cos(angle),
                       s.center.y + s.radius * std::sin(angle)};
}
std::optional<SketchPoint> pointOf(const SketchEllipticArc& s, Position p) { // N14c
    if (p == Position::center)
        return s.center;
    return ellipsePoint(s.ellipse(), p == Position::start ? s.startParameter : s.endParameter);
}
std::optional<SketchPoint> pointOf(const SketchRectangle&, Position) { return std::nullopt; }

} // namespace

std::vector<double> bsplineKnots(const SketchBSpline& spline) {
    if (!spline.knots.empty())
        return spline.knots;
    const auto last = static_cast<double>(spline.poles.size()) - 3.0;
    std::vector<double> knots(4, 0.0);
    for (double knot = 1.0; knot < last; knot += 1.0)
        knots.push_back(knot);
    knots.insert(knots.end(), 4, last);
    return knots;
}

std::optional<SketchPoint> sketchReferencePoint(const SketchGeometry& geometry,
                                                SketchPointPosition point) {
    return std::visit([point](const auto& shape) { return pointOf(shape, point); }, geometry);
}

} // namespace voelund::document
