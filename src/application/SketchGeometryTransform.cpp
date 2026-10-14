#include "SketchGeometryTransform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <type_traits>

namespace voelund::application::detail {
namespace {

using document::SketchConstraintKind;
using document::SketchLineConstraint;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double axisTolerance = 1e-12;

// Where the sketch x axis goes: along x, along y, or neither.
enum class AxisImage { keepsAxes, swapsAxes, oblique };

AxisImage axisImage(const PlanarTransform& t) {
    if (std::abs(t.xy) <= axisTolerance && std::abs(t.yx) <= axisTolerance)
        return AxisImage::keepsAxes;
    if (std::abs(t.xx) <= axisTolerance && std::abs(t.yy) <= axisTolerance)
        return AxisImage::swapsAxes;
    return AxisImage::oblique;
}

SketchLineConstraint mappedMark(SketchLineConstraint mark, AxisImage image) {
    if (mark == SketchLineConstraint::none || image == AxisImage::keepsAxes)
        return image == AxisImage::oblique ? SketchLineConstraint::none : mark;
    if (image == AxisImage::oblique)
        return SketchLineConstraint::none;
    return mark == SketchLineConstraint::horizontal ? SketchLineConstraint::vertical
                                                    : SketchLineConstraint::horizontal;
}

double angleOf(SketchPoint from, SketchPoint to) {
    return std::atan2(to.y - from.y, to.x - from.x);
}

} // namespace

PlanarTransform PlanarTransform::translation(double dx, double dy) {
    return {1.0, 0.0, 0.0, 1.0, dx, dy};
}

PlanarTransform PlanarTransform::rotation(double angle) {
    const double c = std::cos(angle), s = std::sin(angle);
    // Exact quarter turns keep the axes exactly (cos(pi/2) is not 0 in floating point).
    const auto snap = [](double value) { return std::abs(value) < 1e-15 ? 0.0 : value; };
    return {snap(c), snap(-s), snap(s), snap(c), 0.0, 0.0};
}

PlanarTransform PlanarTransform::rotation(SketchPoint center, double angle) {
    PlanarTransform result = rotation(angle);
    const SketchPoint image = result.apply(center);
    result.tx = center.x - image.x;
    result.ty = center.y - image.y;
    return result;
}

PlanarTransform PlanarTransform::scaling(SketchPoint center, double factor) {
    return {factor, 0.0, 0.0, factor, center.x * (1.0 - factor), center.y * (1.0 - factor),
            factor};
}

PlanarTransform PlanarTransform::reflection(const document::SketchLine& axis) {
    const double dx = axis.end.x - axis.start.x, dy = axis.end.y - axis.start.y;
    const double length = std::hypot(dx, dy);
    const double ux = dx / length, uy = dy / length;
    // Householder-style reflection across the axis direction: R = 2uu^T - I.
    const double xx = 2.0 * ux * ux - 1.0, xy = 2.0 * ux * uy, yy = 2.0 * uy * uy - 1.0;
    const auto snap = [](double value) { return std::abs(value) < 1e-15 ? 0.0 : value; };
    PlanarTransform result{snap(xx), snap(xy), snap(xy), snap(yy), 0.0, 0.0};
    const SketchPoint image = result.apply(axis.start);
    result.tx = axis.start.x - image.x;
    result.ty = axis.start.y - image.y;
    return result;
}

SketchPoint PlanarTransform::apply(SketchPoint point) const noexcept {
    return {xx * point.x + xy * point.y + tx, yx * point.x + yy * point.y + ty};
}

bool PlanarTransform::mirrors() const noexcept { return xx * yy - xy * yx < 0.0; }

bool PlanarTransform::keepsAxes() const noexcept {
    return axisImage(*this) == AxisImage::keepsAxes;
}

bool PlanarTransform::translatesOnly() const noexcept {
    return xx == 1.0 && yy == 1.0 && xy == 0.0 && yx == 0.0;
}

TransformedCopy transformedCopy(const document::SketchGeometry& geometry,
                                const PlanarTransform& t) {
    const AxisImage image = axisImage(t);
    TransformedCopy result;
    std::visit(
        [&](const auto& value) {
            using Geometry = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
                result.elements.push_back(document::SketchLine{
                    t.apply(value.start), t.apply(value.end), mappedMark(value.constraint, image)});
            } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                const std::array corners{value.first, SketchPoint{value.second.x, value.first.y},
                                         value.second, SketchPoint{value.first.x, value.second.y}};
                if (image != AxisImage::oblique) {
                    const SketchPoint a = t.apply(value.first), b = t.apply(value.second);
                    result.elements.push_back(document::SketchRectangle{a.x, a.y, b.x, b.y});
                    return;
                }
                // Not representable as a rectangle: four lines, closed and square-cornered.
                for (std::size_t index = 0; index < corners.size(); ++index)
                    result.elements.push_back(document::SketchLine{
                        t.apply(corners[index]), t.apply(corners[(index + 1) % corners.size()])});
                for (std::size_t index = 0; index < corners.size(); ++index)
                    result.constraints.push_back({SketchConstraintKind::coincident, index,
                                                  Position::end, (index + 1) % corners.size(),
                                                  Position::start});
                result.constraints.push_back(
                    {SketchConstraintKind::perpendicular, 0, Position::start, 1, Position::start});
                result.constraints.push_back(
                    {SketchConstraintKind::parallel, 0, Position::start, 2, Position::start});
                result.constraints.push_back(
                    {SketchConstraintKind::parallel, 1, Position::start, 3, Position::start});
            } else if constexpr (std::is_same_v<Geometry, document::SketchArc>) {
                const SketchPoint start{value.center.x + value.radius * std::cos(value.startAngle),
                                        value.center.y + value.radius * std::sin(value.startAngle)};
                const SketchPoint end{value.center.x + value.radius * std::cos(value.endAngle),
                                      value.center.y + value.radius * std::sin(value.endAngle)};
                document::SketchArc arc = value;
                arc.center = t.apply(value.center);
                arc.radius = value.radius * t.scale;
                if (t.translatesOnly()) { // angles stay exactly as they are
                    result.elements.push_back(arc);
                    return;
                }
                // A reflection turns the arc's direction: its old end becomes the new start.
                const SketchPoint newStart = t.apply(t.mirrors() ? end : start);
                const SketchPoint newEnd = t.apply(t.mirrors() ? start : end);
                const double sweep = value.endAngle - value.startAngle;
                arc.startAngle = angleOf(arc.center, newStart);
                arc.endAngle = angleOf(arc.center, newEnd);
                while (arc.endAngle <= arc.startAngle)
                    arc.endAngle += 2.0 * std::numbers::pi;
                if (sweep >= 2.0 * std::numbers::pi - 1e-12)
                    arc.endAngle = arc.startAngle + sweep;
                result.elements.push_back(arc);
            } else if constexpr (std::is_same_v<Geometry, document::SketchPointGeometry>) {
                result.elements.push_back(document::SketchPointGeometry{t.apply(value.position)});
            } else if constexpr (std::is_same_v<Geometry, document::SketchEllipse>) {
                document::SketchEllipse ellipse = value;
                ellipse.center = t.apply(value.center);
                ellipse.majorRadius = value.majorRadius * t.scale;
                ellipse.minorRadius = value.minorRadius * t.scale;
                if (t.translatesOnly()) {
                    result.elements.push_back(ellipse);
                    return;
                }
                const SketchPoint major{std::cos(value.rotation), std::sin(value.rotation)};
                ellipse.rotation =
                    std::atan2(t.yx * major.x + t.yy * major.y, t.xx * major.x + t.xy * major.y);
                result.elements.push_back(ellipse);
            } else if constexpr (std::is_same_v<Geometry, document::SketchBSpline>) {
                document::SketchBSpline spline; // S21: the image of each control point
                for (const auto& pole : value.poles)
                    spline.poles.push_back(t.apply(pole));
                result.elements.push_back(spline);
            } else {
                document::SketchCircle circle = value;
                circle.center = t.apply(value.center);
                circle.radius = value.radius * t.scale;
                result.elements.push_back(circle);
            }
        },
        geometry);
    return result;
}

} // namespace voelund::application::detail
