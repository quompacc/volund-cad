// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace voelund::features {
namespace {

using document::SketchConstraintKind;
using document::SketchPoint;

struct Spot final {
    SketchPoint at;
    double alongX;
    double alongY;
};

const document::SketchElement* findElement(const document::SketchParameters& sketch,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(sketch.elements.values, id, &document::SketchElement::id);
    return match == sketch.elements.values.end() ? nullptr : &*match;
}

Spot onRound(SketchPoint center, double radius, double angle) {
    return {{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)},
            -std::sin(angle),
            std::cos(angle)};
}

// Where an element carries its symbols: the middle of a line, a point on a circle's rim, the
// middle of an arc, the end of an ellipse's major axis; the direction along the curve there.
Spot elementSpot(const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return {{(line->start.x + line->end.x) / 2, (line->start.y + line->end.y) / 2},
                line->end.x - line->start.x,
                line->end.y - line->start.y};
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return onRound(circle->center, circle->radius, std::numbers::pi / 4);
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        double end = arc->endAngle;
        while (end < arc->startAngle)
            end += 2 * std::numbers::pi;
        return onRound(arc->center, arc->radius, (arc->startAngle + end) / 2);
    }
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry))
        return {{ellipse->center.x + ellipse->majorRadius * std::cos(ellipse->rotation),
                 ellipse->center.y + ellipse->majorRadius * std::sin(ellipse->rotation)},
                -std::sin(ellipse->rotation),
                std::cos(ellipse->rotation)};
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&geometry))
        return {{(rectangle->first.x + rectangle->second.x) / 2, rectangle->first.y}, 1, 0};
    if (const auto* point = std::get_if<document::SketchPointGeometry>(&geometry))
        return {point->position, 1, 0};
    if (const auto* arc = std::get_if<document::SketchEllipticArc>(&geometry)) { // N14c
        const double middle = (arc->startParameter + arc->endParameter) / 2;
        const auto a = document::ellipsePoint(arc->ellipse(), middle - 1e-3);
        const auto b = document::ellipsePoint(arc->ellipse(), middle + 1e-3);
        return {document::ellipsePoint(arc->ellipse(), middle), b.x - a.x, b.y - a.y};
    }
    const auto& poles = std::get<document::SketchBSpline>(geometry).poles;
    return {poles.empty() ? SketchPoint{0, 0} : poles[poles.size() / 2], 1, 0};
}

// Relations between two curves show their symbol on each of them (W18, as SolidCraft).
bool onBothCurves(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::parallel:
    case SketchConstraintKind::perpendicular:
    case SketchConstraintKind::collinear:
    case SketchConstraintKind::equal:
    case SketchConstraintKind::tangent:
    case SketchConstraintKind::concentric:
        return true;
    default:
        return false;
    }
}

// A curve's direction (horizontal, vertical, a bisector) sits on that curve.
bool onOneCurve(const document::SketchConstraint& constraint) {
    switch (constraint.kind) {
    case SketchConstraintKind::horizontal:
    case SketchConstraintKind::vertical:
    case SketchConstraintKind::perpendicularBisector:
        return !constraint.second.has_value();
    default:
        return false;
    }
}

class Plane final {
  public:
    explicit Plane(const document::SketchPlane& plane)
        : origin_(plane.originX, plane.originY, plane.originZ),
          normal_(plane.normalX, plane.normalY, plane.normalZ),
          x_(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ), y_(normal_.Crossed(x_)) {}
    [[nodiscard]] SketchGlyphAnchor anchor(const Spot& spot,
                                           std::vector<document::SketchElementId> elements) const {
        constexpr double displayOffset = 0.02; // as the annotation points: just above the plane
        const gp_Pnt at = origin_.Translated(gp_Vec(x_) * spot.at.x + gp_Vec(y_) * spot.at.y +
                                             gp_Vec(normal_) * displayOffset);
        const gp_Vec along = gp_Vec(x_) * spot.alongX + gp_Vec(y_) * spot.alongY;
        return {at, along.Magnitude() > 1e-12 ? gp_Dir(along) : gp_Dir(x_), std::move(elements)};
    }
    [[nodiscard]] gp_Dir xDirection() const { return x_; }

  private:
    gp_Pnt origin_;
    gp_Dir normal_;
    gp_Dir x_;
    gp_Dir y_;
};

} // namespace

std::vector<SketchGlyphAnchor> sketchGlyphAnchors(const document::SketchParameters& sketch,
                                                  const document::SketchConstraint& constraint,
                                                  const gp_Pnt& annotationPosition) {
    std::vector<SketchGlyphAnchor> result;
    if (sketchAnnotationCategory(constraint.kind) != SketchAnnotationCategory::relation)
        return result;
    const Plane plane(sketch.plane);
    std::vector<document::SketchElementId> elements{constraint.first.element};
    if (constraint.second)
        elements.push_back(constraint.second->element);
    const auto* first = findElement(sketch, constraint.first.element);
    const auto* second =
        constraint.second ? findElement(sketch, constraint.second->element) : nullptr;
    if (onBothCurves(constraint.kind) && first != nullptr && second != nullptr) {
        result.push_back(plane.anchor(elementSpot(first->geometry), elements));
        result.push_back(plane.anchor(elementSpot(second->geometry), elements));
        return result;
    }
    if (onOneCurve(constraint) && first != nullptr) {
        result.push_back(plane.anchor(elementSpot(first->geometry), elements));
        return result;
    }
    // A relation of points sits at its point (or between its points).
    result.push_back({annotationPosition, plane.xDirection(), std::move(elements)});
    return result;
}

} // namespace voelund::features
