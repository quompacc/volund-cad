#include <voelund/features/SketchConstraintSolver.hpp>

#include <cmath>
#include <optional>

// S18 (user decision 2026-10-14): a reference dimension determines nothing; after each solve
// it shows what the geometry measures.
namespace voelund::features {
namespace {

using document::SketchConstraintKind;
using document::SketchPoint;

const document::SketchElement* find(const document::SketchElements& elements,
                                    document::SketchElementId id) {
    for (const auto& element : elements.values)
        if (element.id == id)
            return &element;
    return nullptr;
}

std::optional<SketchPoint> pointOf(const document::SketchElements& elements,
                                   const document::SketchGeometryReference& reference) {
    const auto* element = find(elements, reference.element);
    if (element == nullptr)
        return std::nullopt;
    const bool start = reference.point == document::SketchPointPosition::start;
    if (const auto* line = std::get_if<document::SketchLine>(&element->geometry))
        return start ? line->start : line->end;
    if (const auto* circle = std::get_if<document::SketchCircle>(&element->geometry))
        return circle->center;
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&element->geometry))
        return ellipse->center;
    if (const auto* point = std::get_if<document::SketchPointGeometry>(&element->geometry))
        return point->position;
    if (const auto* spline = std::get_if<document::SketchBSpline>(&element->geometry))
        return start ? spline->poles.front() : spline->poles.back(); // S21
    if (const auto* arc = std::get_if<document::SketchArc>(&element->geometry)) {
        if (reference.point == document::SketchPointPosition::center)
            return arc->center;
        const double angle = start ? arc->startAngle : arc->endAngle;
        return SketchPoint{arc->center.x + arc->radius * std::cos(angle),
                           arc->center.y + arc->radius * std::sin(angle)};
    }
    return std::nullopt;
}

const document::SketchLine* lineOf(const document::SketchElements& elements,
                                   document::SketchElementId id) {
    const auto* element = find(elements, id);
    return element == nullptr ? nullptr : std::get_if<document::SketchLine>(&element->geometry);
}

std::optional<double> radiusOf(const document::SketchElements& elements,
                               document::SketchElementId id) {
    const auto* element = find(elements, id);
    if (element == nullptr)
        return std::nullopt;
    if (const auto* circle = std::get_if<document::SketchCircle>(&element->geometry))
        return circle->radius;
    if (const auto* arc = std::get_if<document::SketchArc>(&element->geometry))
        return arc->radius;
    return std::nullopt;
}

} // namespace

bool isSketchDimension(SketchConstraintKind kind) noexcept {
    switch (kind) {
    case SketchConstraintKind::distance:
    case SketchConstraintKind::radius:
    case SketchConstraintKind::diameter:
    case SketchConstraintKind::pointDistance:
    case SketchConstraintKind::pointLineDistance:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
    case SketchConstraintKind::angle:
        return true;
    default:
        return false;
    }
}

void measureReferenceDimensions(document::SketchParameters& sketch) {
    const auto& elements = sketch.elements;
    for (auto& constraint : sketch.constraints.values) {
        if (!constraint.reference)
            continue;
        const auto kind = constraint.kind;
        if (kind == SketchConstraintKind::distance) {
            if (const auto* line = lineOf(elements, constraint.first.element))
                constraint.value =
                    std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
        } else if (kind == SketchConstraintKind::radius || kind == SketchConstraintKind::diameter) {
            if (const auto radius = radiusOf(elements, constraint.first.element))
                constraint.value = kind == SketchConstraintKind::diameter ? 2 * *radius : *radius;
        } else if (kind == SketchConstraintKind::angle) {
            const auto* first = lineOf(elements, constraint.first.element);
            const auto* second =
                constraint.second ? lineOf(elements, constraint.second->element) : nullptr;
            if (first != nullptr && second != nullptr) {
                const double ax = first->end.x - first->start.x, ay = first->end.y - first->start.y;
                const double bx = second->end.x - second->start.x,
                             by = second->end.y - second->start.y;
                constraint.angleValue =
                    kernel::Radians(std::atan2(ax * by - ay * bx, ax * bx + ay * by));
            }
        } else if (kind == SketchConstraintKind::pointLineDistance) {
            const auto point = pointOf(elements, constraint.first);
            const auto* line =
                constraint.second ? lineOf(elements, constraint.second->element) : nullptr;
            if (point && line != nullptr) {
                const double x = line->end.x - line->start.x, y = line->end.y - line->start.y;
                const double length = std::hypot(x, y);
                if (length > 0)
                    constraint.value =
                        std::abs(x * (point->y - line->start.y) - y * (point->x - line->start.x)) /
                        length;
            }
        } else if (constraint.second) {
            const auto first = pointOf(elements, constraint.first);
            const auto second = pointOf(elements, *constraint.second);
            if (!first || !second)
                continue;
            constraint.value = kind == SketchConstraintKind::pointDistanceX ? second->x - first->x
                               : kind == SketchConstraintKind::pointDistanceY
                                   ? second->y - first->y
                                   : std::hypot(second->x - first->x, second->y - first->y);
        }
    }
}

} // namespace voelund::features
