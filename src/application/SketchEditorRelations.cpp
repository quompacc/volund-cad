#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/kernel/Angle.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>
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

bool arc(const document::SketchElement* element) {
    return element && std::holds_alternative<document::SketchArc>(element->geometry);
}

bool radial(const document::SketchElement* element) { return circle(element) || arc(element); }

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
    if (arc(element)) {
        const auto& geometry = std::get<document::SketchArc>(element->geometry);
        if (reference.point == document::SketchPointPosition::center)
            return geometry.center;
        const double angle = reference.point == document::SketchPointPosition::start
                                 ? geometry.startAngle
                                 : geometry.endAngle;
        return document::SketchPoint{geometry.center.x + geometry.radius * std::cos(angle),
                                     geometry.center.y + geometry.radius * std::sin(angle)};
    }
    return std::nullopt;
}

kernel::Radians lineAngleRadians(const document::SketchLine& first,
                                 const document::SketchLine& second) {
    const double firstX = first.end.x - first.start.x;
    const double firstY = first.end.y - first.start.y;
    const double secondX = second.end.x - second.start.x;
    const double secondY = second.end.y - second.start.y;
    return kernel::Radians(
        std::atan2(firstX * secondY - firstY * secondX, firstX * secondX + firstY * secondY));
}

double pointLineDistance(const document::SketchPoint& point, const document::SketchLine& line) {
    const double x = line.end.x - line.start.x;
    const double y = line.end.y - line.start.y;
    return std::abs(x * (point.y - line.start.y) - y * (point.x - line.start.x)) / std::hypot(x, y);
}

} // namespace

bool SketchEditorSession::canAddSelectedRelation(
    document::SketchConstraintKind kind) const noexcept {
    if (!active_)
        return false;
    if (kind == document::SketchConstraintKind::diameter ||
        kind == document::SketchConstraintKind::radius ||
        kind == document::SketchConstraintKind::fixed) {
        if (selectedReferences_.size() != 1)
            return false;
        const auto* element = findElement(elements_, selectedReferences_.front().element);
        if (kind == document::SketchConstraintKind::diameter)
            return radial(element);
        // S16: one radius per element; the panel's radius field edits the same constraint.
        if (kind == document::SketchConstraintKind::radius)
            return radial(element) &&
                   std::ranges::none_of(constraints_.values, [&](const auto& constraint) {
                       return constraint.kind == document::SketchConstraintKind::radius &&
                              constraint.first.element == element->id;
                   });
        return (line(element) || circle(element)) &&
               std::ranges::none_of(constraints_.values, [&](const auto& constraint) {
                   return constraint.kind == document::SketchConstraintKind::fixed &&
                          constraint.first.element == element->id;
               });
    }
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
    if (kind == document::SketchConstraintKind::horizontalPoints ||
        kind == document::SketchConstraintKind::verticalPoints)
        return referencePoint(elements_, first).has_value() &&
               referencePoint(elements_, second).has_value();
    if (kind == document::SketchConstraintKind::pointDistance) {
        const auto firstPoint = referencePoint(elements_, first);
        const auto secondPoint = referencePoint(elements_, second);
        return firstPoint.has_value() && secondPoint.has_value() &&
               std::hypot(secondPoint->x - firstPoint->x, secondPoint->y - firstPoint->y) > 1.0e-9;
    }
    if (kind == document::SketchConstraintKind::pointLineDistance) {
        const auto firstPoint = referencePoint(elements_, first);
        const auto* target = findElement(elements_, second.element);
        return first.element != second.element && firstPoint.has_value() && line(target) &&
               pointLineDistance(*firstPoint, std::get<document::SketchLine>(target->geometry)) >
                   1.0e-9;
    }
    if (kind == document::SketchConstraintKind::coincident)
        return true;
    if (first.element == second.element)
        return false;
    const document::SketchElement* firstElement = findElement(elements_, first.element);
    const document::SketchElement* secondElement = findElement(elements_, second.element);
    if (kind == document::SketchConstraintKind::parallel ||
        kind == document::SketchConstraintKind::perpendicular ||
        kind == document::SketchConstraintKind::perpendicularBisector ||
        kind == document::SketchConstraintKind::collinear)
        return line(firstElement) && line(secondElement);
    if (kind == document::SketchConstraintKind::midpoint)
        return referencePoint(elements_, first).has_value() && line(secondElement);
    if (kind == document::SketchConstraintKind::angle) {
        if (!line(firstElement) || !line(secondElement))
            return false;
        const auto value =
            lineAngleRadians(std::get<document::SketchLine>(firstElement->geometry),
                             std::get<document::SketchLine>(secondElement->geometry));
        return std::abs(value.value()) > kernel::toRadians(kernel::Degrees(1.0e-9)).value() &&
               std::abs(value.value()) < std::numbers::pi;
    }
    if (kind == document::SketchConstraintKind::pointOnObject)
        return (line(firstElement) || circle(firstElement)) &&
               (line(secondElement) || circle(secondElement));
    if (kind == document::SketchConstraintKind::equal)
        return (line(firstElement) && line(secondElement)) ||
               (radial(firstElement) && radial(secondElement));
    if (kind == document::SketchConstraintKind::tangent)
        return (line(firstElement) && radial(secondElement)) ||
               (radial(firstElement) && line(secondElement)) ||
               (radial(firstElement) && radial(secondElement));
    if (kind == document::SketchConstraintKind::concentric)
        return radial(firstElement) && radial(secondElement);
    return false;
}

bool SketchEditorSession::addSelectedRelation(document::SketchConstraintKind kind) {
    if (!canAddSelectedRelation(kind) || !canAllocateIds(0, 1))
        return false;
    Rollback rollback(*this);
    std::optional<document::SketchGeometryReference> secondReference;
    if (kind != document::SketchConstraintKind::diameter &&
        kind != document::SketchConstraintKind::radius &&
        kind != document::SketchConstraintKind::fixed)
        secondReference = selectedReferences_[1];
    document::SketchConstraint constraint{document::SketchConstraintId{constraints_.nextId++}, kind,
                                          selectedReferences_[0], secondReference, 0.0};
    if (kind == document::SketchConstraintKind::diameter ||
        kind == document::SketchConstraintKind::radius) {
        const auto* element = findElement(elements_, selectedReferences_[0].element);
        const double factor = kind == document::SketchConstraintKind::diameter ? 2.0 : 1.0;
        if (const auto* circleGeometry = std::get_if<document::SketchCircle>(&element->geometry))
            constraint.value = circleGeometry->radius * factor;
        else
            constraint.value = std::get<document::SketchArc>(element->geometry).radius * factor;
    } else if (kind == document::SketchConstraintKind::fixed) {
        const auto* element = findElement(elements_, selectedReferences_[0].element);
        if (const auto* geometry = std::get_if<document::SketchLine>(&element->geometry)) {
            constraint.value = geometry->start.x;
            constraint.auxiliaryValues = {geometry->start.y, geometry->end.x, geometry->end.y};
        } else {
            const auto& circleGeometry = std::get<document::SketchCircle>(element->geometry);
            constraint.value = circleGeometry.center.x;
            constraint.auxiliaryValues = {circleGeometry.center.y, circleGeometry.radius};
        }
    }
    if (kind == document::SketchConstraintKind::pointDistance) {
        const auto first = referencePoint(elements_, selectedReferences_[0]);
        const auto second = referencePoint(elements_, selectedReferences_[1]);
        constraint.value = std::hypot(second->x - first->x, second->y - first->y);
    } else if (kind == document::SketchConstraintKind::pointLineDistance) {
        const auto source = referencePoint(elements_, selectedReferences_[0]);
        const auto* target = findElement(elements_, selectedReferences_[1].element);
        constraint.value =
            pointLineDistance(*source, std::get<document::SketchLine>(target->geometry));
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
        constraint.angleValue = lineAngleRadians(std::get<document::SketchLine>(first->geometry),
                                                 std::get<document::SketchLine>(second->geometry));
    }
    if (kind == document::SketchConstraintKind::symmetric)
        constraint.third = selectedReferences_[2];
    // A conflicting relation stays visible so it can be removed; canAccept is false meanwhile.
    // An invalid one is not added (S02).
    constraints_.values.push_back(constraint);
    try {
        document::validateSketchConstraints(elements_, constraints_);
    } catch (const std::invalid_argument&) {
        return false;
    }
    static_cast<void>(solveConstraints());
    rollback.keep();
    return true;
}

bool SketchEditorSession::addCoincidentReferences(document::SketchGeometryReference first,
                                                  document::SketchGeometryReference second) {
    if (!active_ || first == second)
        return false;
    if (std::ranges::any_of(constraints_.values, [first, second](const auto& constraint) {
            return constraint.kind == document::SketchConstraintKind::coincident &&
                   constraint.second.has_value() &&
                   ((constraint.first == first && *constraint.second == second) ||
                    (constraint.first == second && *constraint.second == first));
        }))
        return false;
    if (!canAllocateIds(0, 1))
        return false;
    const auto previousElements = elements_;
    const features::SketchSolveResult previousSolve = lastSolveResult_;
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coincident, first, second, 0.0});
    try {
        document::validateSketchConstraints(elements_, constraints_);
    } catch (const std::invalid_argument&) {
        constraints_.values.pop_back();
        --constraints_.nextId;
        return false;
    }
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    lastSolveResult_ = previousSolve;
    constraints_.values.pop_back();
    --constraints_.nextId;
    return false;
}

bool SketchEditorSession::addAxisReference(document::SketchGeometryReference reference,
                                           SketchAxisReference axis) {
    if (!active_ || reference.element.value() == 0)
        return false;
    const auto missing = [&](document::SketchConstraintKind kind) {
        return std::ranges::none_of(constraints_.values, [&](const auto& item) {
            return item.kind == kind && item.first == reference && !item.second.has_value() &&
                   item.value == 0.0;
        });
    };
    const std::uint64_t needed =
        ((axis == SketchAxisReference::origin || axis == SketchAxisReference::yAxis) &&
         missing(document::SketchConstraintKind::coordinateX)) +
        ((axis == SketchAxisReference::origin || axis == SketchAxisReference::xAxis) &&
         missing(document::SketchConstraintKind::coordinateY));
    if (!canAllocateIds(0, needed))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    const auto addCoordinate = [this, reference](document::SketchConstraintKind kind) {
        const auto duplicate = std::ranges::find_if(constraints_.values, [&](const auto& item) {
            return item.kind == kind && item.first == reference && !item.second.has_value() &&
                   item.value == 0.0;
        });
        if (duplicate == constraints_.values.end())
            constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                           kind, reference, std::nullopt, 0.0});
    };
    if (axis == SketchAxisReference::origin || axis == SketchAxisReference::yAxis)
        addCoordinate(document::SketchConstraintKind::coordinateX);
    if (axis == SketchAxisReference::origin || axis == SketchAxisReference::xAxis)
        addCoordinate(document::SketchConstraintKind::coordinateY);
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

bool SketchEditorSession::addMidpointReference(document::SketchGeometryReference reference,
                                               document::SketchElementId line) {
    if (!active_ || reference.element == line)
        return false;
    if (!canAllocateIds(0, 1))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++},
         document::SketchConstraintKind::midpoint, reference,
         document::SketchGeometryReference{line, document::SketchPointPosition::start}});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

bool SketchEditorSession::addFixedReference(document::SketchGeometryReference reference,
                                            document::SketchPoint point) {
    if (!active_ || reference.element.value() == 0)
        return false;
    if (!canAllocateIds(0, 2))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coordinateX, reference,
                                   std::nullopt, point.x});
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coordinateY, reference,
                                   std::nullopt, point.y});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

bool SketchEditorSession::addSupportReference(document::SketchGeometryReference reference,
                                              document::SketchPoint point,
                                              std::uint64_t supportReferenceId,
                                              double supportReferenceParameter) {
    if (supportBinding_ || !active_ || reference.element.value() == 0 || supportReferenceId == 0 ||
        !std::isfinite(supportReferenceParameter))
        return false;
    if (!canAllocateIds(0, 2))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    const std::vector<double> attachment{static_cast<double>(supportReferenceId),
                                         std::clamp(supportReferenceParameter, 0.0, 1.0)};
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coordinateX, reference,
                                   std::nullopt, point.x, std::nullopt, attachment});
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coordinateY, reference,
                                   std::nullopt, point.y, std::nullopt, attachment});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

bool SketchEditorSession::addPointOnObjectReference(document::SketchGeometryReference reference,
                                                    document::SketchElementId object) {
    if (!active_ || reference.element == object ||
        !referencePoint(elements_, reference).has_value() || !line(findElement(elements_, object)))
        return false;
    const document::SketchGeometryReference target{object, document::SketchPointPosition::start};
    if (std::ranges::any_of(constraints_.values, [&](const auto& constraint) {
            return constraint.kind == document::SketchConstraintKind::pointOnObject &&
                   constraint.first == reference && constraint.second == target;
        }))
        return true;
    if (!canAllocateIds(0, 1))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::pointOnObject, reference,
                                   target});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

std::optional<document::SketchConstraintKind>
SketchEditorSession::addAutomaticLineRelation(document::SketchElementId lineId) {
    const auto* createdElement = findElement(elements_, lineId);
    if (!active_ || !line(createdElement))
        return std::nullopt;
    const auto& created = std::get<document::SketchLine>(createdElement->geometry);
    const double createdX = created.end.x - created.start.x;
    const double createdY = created.end.y - created.start.y;
    const double createdLength = std::hypot(createdX, createdY);
    document::SketchElementId targetId{0};
    document::SketchConstraintKind relation = document::SketchConstraintKind::parallel;
    double best = 0.08;
    for (const auto& candidate : elements_.values) {
        const auto* target = std::get_if<document::SketchLine>(&candidate.geometry);
        if (candidate.id == lineId || target == nullptr)
            continue;
        const double targetX = target->end.x - target->start.x;
        const double targetY = target->end.y - target->start.y;
        const double scale = createdLength * std::hypot(targetX, targetY);
        if (scale <= 1.0e-12)
            continue;
        const double parallelError = std::abs(createdX * targetY - createdY * targetX) / scale;
        const double perpendicularError = std::abs(createdX * targetX + createdY * targetY) / scale;
        if (parallelError < best) {
            best = parallelError;
            targetId = candidate.id;
            relation = document::SketchConstraintKind::parallel;
        }
        if (perpendicularError < best) {
            best = perpendicularError;
            targetId = candidate.id;
            relation = document::SketchConstraintKind::perpendicular;
        }
    }
    if (targetId.value() == 0)
        return std::nullopt;
    if (!canAllocateIds(0, 1))
        return std::nullopt;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++},
         relation,
         {lineId, document::SketchPointPosition::start},
         document::SketchGeometryReference{targetId, document::SketchPointPosition::start}});
    if (solveConstraints())
        return relation;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return std::nullopt;
}

bool SketchEditorSession::addTangencyReference(document::SketchElementId radialId,
                                               document::SketchElementId targetId) {
    const auto* radialElement = findElement(elements_, radialId);
    const auto* targetElement = findElement(elements_, targetId);
    if (!active_ || !radial(radialElement) || (!line(targetElement) && !radial(targetElement)) ||
        radialId == targetId)
        return false;
    if (!canAllocateIds(0, 1))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++},
         document::SketchConstraintKind::tangent,
         {radialId, document::SketchPointPosition::start},
         document::SketchGeometryReference{targetId, document::SketchPointPosition::start}});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

bool SketchEditorSession::addAutomaticRadialRelation(document::SketchElementId radialId,
                                                     document::SketchElementId targetId,
                                                     document::SketchConstraintKind kind) {
    if (kind != document::SketchConstraintKind::equal &&
        kind != document::SketchConstraintKind::concentric &&
        kind != document::SketchConstraintKind::tangent)
        return false;
    const auto* radialElement = findElement(elements_, radialId);
    const auto* targetElement = findElement(elements_, targetId);
    if (!active_ || radialId == targetId || !radial(radialElement) || !radial(targetElement))
        return false;
    if (std::ranges::any_of(constraints_.values, [&](const auto& constraint) {
            return constraint.kind == kind && constraint.first.element == radialId &&
                   constraint.second.has_value() && constraint.second->element == targetId;
        }))
        return true;
    if (!canAllocateIds(0, 1))
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++},
         kind,
         {radialId, document::SketchPointPosition::center},
         document::SketchGeometryReference{targetId, document::SketchPointPosition::center}});
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}

} // namespace voelund::application
