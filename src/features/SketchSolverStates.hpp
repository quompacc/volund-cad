#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <GCS.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <unordered_map>
#include <variant>
#include <vector>

// The solver's view of each sketch element: its unknowns and how they go back into the
// geometry (SketchConstraintSolver.cpp).
namespace voelund::features::detail {

struct LineState final {
    document::SketchElement* element;
    double firstX;
    double firstY;
    double secondX;
    double secondY;
    GCS::Point first;
    GCS::Point second;
    GCS::Line line;

    explicit LineState(document::SketchElement& source)
        : element(&source), firstX(std::get<document::SketchLine>(source.geometry).start.x),
          firstY(std::get<document::SketchLine>(source.geometry).start.y),
          secondX(std::get<document::SketchLine>(source.geometry).end.x),
          secondY(std::get<document::SketchLine>(source.geometry).end.y), first{&firstX, &firstY},
          second{&secondX, &secondY} {
        line.p1 = first;
        line.p2 = second;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&firstX, &firstY, &secondX, &secondY});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchLine>(element->geometry);
        target.start = {firstX, firstY};
        target.end = {secondX, secondY};
    }
};

struct CircleState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double radius;
    GCS::Circle circle;

    explicit CircleState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchCircle>(source.geometry).center.x),
          centerY(std::get<document::SketchCircle>(source.geometry).center.y),
          radius(std::get<document::SketchCircle>(source.geometry).radius) {
        circle.center = {&centerX, &centerY};
        circle.rad = &radius;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &radius});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchCircle>(element->geometry);
        target.center = {centerX, centerY};
        target.radius = radius;
    }
};

struct ArcState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double radius;
    double startX;
    double startY;
    double endX;
    double endY;
    double startAngle;
    double endAngle;
    GCS::Arc arc;

    explicit ArcState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchArc>(source.geometry).center.x),
          centerY(std::get<document::SketchArc>(source.geometry).center.y),
          radius(std::get<document::SketchArc>(source.geometry).radius),
          startAngle(std::get<document::SketchArc>(source.geometry).startAngle),
          endAngle(std::get<document::SketchArc>(source.geometry).endAngle) {
        startX = centerX + radius * std::cos(startAngle);
        startY = centerY + radius * std::sin(startAngle);
        endX = centerX + radius * std::cos(endAngle);
        endY = centerY + radius * std::sin(endAngle);
        arc.center = {&centerX, &centerY};
        arc.rad = &radius;
        arc.start = {&startX, &startY};
        arc.end = {&endX, &endY};
        arc.startAngle = &startAngle;
        arc.endAngle = &endAngle;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &radius, &startX, &startY, &endX,
                                         &endY, &startAngle, &endAngle});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchArc>(element->geometry);
        target.center = {centerX, centerY};
        target.radius = radius;
        target.startAngle = std::remainder(startAngle, 2.0 * std::numbers::pi);
        double sweep = std::fmod(endAngle - startAngle, 2.0 * std::numbers::pi);
        if (sweep <= 1.0e-9)
            sweep += 2.0 * std::numbers::pi;
        target.endAngle = target.startAngle + sweep;
    }
};

struct EllipseState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double focusX;
    double focusY;
    double minorRadius;
    GCS::Ellipse ellipse;

    explicit EllipseState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchEllipse>(source.geometry).center.x),
          centerY(std::get<document::SketchEllipse>(source.geometry).center.y),
          minorRadius(std::get<document::SketchEllipse>(source.geometry).minorRadius) {
        const auto& value = std::get<document::SketchEllipse>(source.geometry);
        const double focalDistance = std::sqrt(value.majorRadius * value.majorRadius -
                                               value.minorRadius * value.minorRadius);
        focusX = centerX + focalDistance * std::cos(value.rotation);
        focusY = centerY + focalDistance * std::sin(value.rotation);
        ellipse.center = {&centerX, &centerY};
        ellipse.focus1 = {&focusX, &focusY};
        ellipse.radmin = &minorRadius;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &focusX, &focusY, &minorRadius});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchEllipse>(element->geometry);
        target.center = {centerX, centerY};
        target.minorRadius = std::abs(minorRadius);
        target.majorRadius =
            std::hypot(std::hypot(focusX - centerX, focusY - centerY), target.minorRadius);
        target.rotation = std::atan2(focusY - centerY, focusX - centerX);
    }
};

// S15: a single point; it is referenced as its center.
struct PointState final {
    document::SketchElement* element;
    double x;
    double y;
    GCS::Point point;

    explicit PointState(document::SketchElement& source)
        : element(&source), x(std::get<document::SketchPointGeometry>(source.geometry).position.x),
          y(std::get<document::SketchPointGeometry>(source.geometry).position.y) {
        point = {&x, &y};
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) { unknowns.insert(unknowns.end(), {&x, &y}); }

    void writeBack() const {
        std::get<document::SketchPointGeometry>(element->geometry).position = {x, y};
    }
};

// S21: an open cubic B-spline; every control point is an unknown. Its ends (the first and
// last control point, clamped) are referenced as start and end.
struct BSplineState final {
    document::SketchElement* element;
    std::vector<double> coordinates;
    std::vector<GCS::Point> poles;
    explicit BSplineState(document::SketchElement& source) : element(&source) {
        const auto& value = std::get<document::SketchBSpline>(source.geometry);
        for (const auto& pole : value.poles)
            coordinates.insert(coordinates.end(), {pole.x, pole.y});
        for (std::size_t index = 0; index < value.poles.size(); ++index)
            poles.push_back({&coordinates[2 * index], &coordinates[2 * index + 1]});
    }
    void appendUnknowns(GCS::VEC_pD& unknowns) {
        for (auto& value : coordinates)
            unknowns.push_back(&value);
    }
    void writeBack() const {
        auto& target = std::get<document::SketchBSpline>(element->geometry);
        for (std::size_t index = 0; index < target.poles.size(); ++index)
            target.poles[index] = {coordinates[2 * index], coordinates[2 * index + 1]};
    }
};
// The key of a B-spline end in the point map (element IDs stay far below these bits).
inline std::uint64_t bsplineEndKey(std::uint64_t element, bool start) {
    return element | (start ? (std::uint64_t{1} << 62) : (std::uint64_t{1} << 63));
}
// PlaneGCS orders unknowns by their addresses. All states share one storage in element order so
// an underdetermined sketch solves the same way in every run, independent of heap layout.
using ElementState =
    std::variant<LineState, CircleState, ArcState, EllipseState, PointState, BSplineState>;

using LineMap = std::unordered_map<std::uint64_t, LineState*>;
using CircleMap = std::unordered_map<std::uint64_t, CircleState*>;
using ArcMap = std::unordered_map<std::uint64_t, ArcState*>;
using PointMap = std::unordered_map<std::uint64_t, GCS::Point*>;

} // namespace voelund::features::detail
