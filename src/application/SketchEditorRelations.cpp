#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

namespace voelund::application {
namespace {

const document::SketchElement* findElement(const document::SketchElements& elements,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(elements.values, id, &document::SketchElement::id);
    return match == elements.values.end() ? nullptr : &*match;
}

bool line(const document::SketchElement* element) {
    return element && std::holds_alternative<document::SketchLine>(element->geometry);
}

bool circle(const document::SketchElement* element) {
    return element && std::holds_alternative<document::SketchCircle>(element->geometry);
}

std::optional<document::SketchPoint>
referencePoint(const document::SketchElements& elements,
               const document::SketchGeometryReference& reference) {
    const document::SketchElement* element = findElement(elements, reference.element);
    if (line(element)) {
        const auto& geometry = std::get<document::SketchLine>(element->geometry);
        return reference.point == document::SketchPointPosition::end ? geometry.end
                                                                     : geometry.start;
    }
    if (circle(element) && reference.point == document::SketchPointPosition::center)
        return std::get<document::SketchCircle>(element->geometry).center;
    return std::nullopt;
}

double lineAngleDegrees(const document::SketchLine& first, const document::SketchLine& second) {
    const double firstX = first.end.x - first.start.x;
    const double firstY = first.end.y - first.start.y;
    const double secondX = second.end.x - second.start.x;
    const double secondY = second.end.y - second.start.y;
    return std::atan2(firstX * secondY - firstY * secondX, firstX * secondX + firstY * secondY) *
           180.0 / std::numbers::pi;
}

} // namespace

bool SketchEditorSession::canAddSelectedRelation(
    document::SketchConstraintKind kind) const noexcept {
    if (!active_)
        return false;
    if (kind == document::SketchConstraintKind::symmetric) {
        if (selectedReferences_.size() != 3)
            return false;
        const auto& first = selectedReferences_[0];
        const auto& second = selectedReferences_[1];
        const auto& axis = selectedReferences_[2];
        return first != second && axis.element != first.element && axis.element != second.element &&
               line(findElement(elements_, axis.element));
    }
    if (selectedReferences_.size() != 2)
        return false;
    const auto& first = selectedReferences_[0];
    const auto& second = selectedReferences_[1];
    if (first == second)
        return false;
    if (kind == document::SketchConstraintKind::pointDistanceX ||
        kind == document::SketchConstraintKind::pointDistanceY)
        return referencePoint(elements_, first).has_value() &&
               referencePoint(elements_, second).has_value();
    if (kind == document::SketchConstraintKind::pointDistance) {
        const auto firstPoint = referencePoint(elements_, first);
        const auto secondPoint = referencePoint(elements_, second);
        return firstPoint.has_value() && secondPoint.has_value() &&
               std::hypot(secondPoint->x - firstPoint->x, secondPoint->y - firstPoint->y) > 1.0e-9;
    }
    if (kind == document::SketchConstraintKind::coincident)
        return true;
    if (first.element == second.element)
        return false;
    const document::SketchElement* firstElement = findElement(elements_, first.element);
    const document::SketchElement* secondElement = findElement(elements_, second.element);
    if (kind == document::SketchConstraintKind::parallel ||
        kind == document::SketchConstraintKind::perpendicular ||
        kind == document::SketchConstraintKind::perpendicularBisector)
        return line(firstElement) && line(secondElement);
    if (kind == document::SketchConstraintKind::midpoint)
        return referencePoint(elements_, first).has_value() && line(secondElement);
    if (kind == document::SketchConstraintKind::angle) {
        if (!line(firstElement) || !line(secondElement))
            return false;
        const double value =
            lineAngleDegrees(std::get<document::SketchLine>(firstElement->geometry),
                             std::get<document::SketchLine>(secondElement->geometry));
        return std::abs(value) > 1.0e-9 && std::abs(value) < 180.0;
    }
    if (kind == document::SketchConstraintKind::pointOnObject)
        return (line(firstElement) || circle(firstElement)) &&
               (line(secondElement) || circle(secondElement));
    if (kind == document::SketchConstraintKind::equal)
        return (line(firstElement) && line(secondElement)) ||
               (circle(firstElement) && circle(secondElement));
    if (kind == document::SketchConstraintKind::tangent)
        return (line(firstElement) && circle(secondElement)) ||
               (circle(firstElement) && line(secondElement)) ||
               (circle(firstElement) && circle(secondElement));
    return false;
}

bool SketchEditorSession::addSelectedRelation(document::SketchConstraintKind kind) {
    if (!canAddSelectedRelation(kind))
        return false;
    document::SketchConstraint constraint{document::SketchConstraintId{constraints_.nextId++}, kind,
                                          selectedReferences_[0], selectedReferences_[1], 0.0};
    if (kind == document::SketchConstraintKind::pointDistance) {
        const auto first = referencePoint(elements_, selectedReferences_[0]);
        const auto second = referencePoint(elements_, selectedReferences_[1]);
        constraint.value = std::hypot(second->x - first->x, second->y - first->y);
    } else if (kind == document::SketchConstraintKind::pointDistanceX ||
               kind == document::SketchConstraintKind::pointDistanceY) {
        const auto first = referencePoint(elements_, selectedReferences_[0]);
        const auto second = referencePoint(elements_, selectedReferences_[1]);
        constraint.value = kind == document::SketchConstraintKind::pointDistanceX
                               ? second->x - first->x
                               : second->y - first->y;
    } else if (kind == document::SketchConstraintKind::angle) {
        const auto* first = findElement(elements_, selectedReferences_[0].element);
        const auto* second = findElement(elements_, selectedReferences_[1].element);
        constraint.value = lineAngleDegrees(std::get<document::SketchLine>(first->geometry),
                                            std::get<document::SketchLine>(second->geometry));
    }
    if (kind == document::SketchConstraintKind::symmetric)
        constraint.third = selectedReferences_[2];
    constraints_.values.push_back(constraint);
    document::SketchParameters sketch{plane_, elements_, constraints_};
    lastSolveResult_ = features::solveSketchConstraints(sketch);
    if (lastSolveResult_.status == features::SketchSolveStatus::success)
        elements_ = std::move(sketch.elements);
    return true;
}

} // namespace voelund::application
