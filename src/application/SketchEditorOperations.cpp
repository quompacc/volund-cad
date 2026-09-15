#include <voelund/document/IdCapacity.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

namespace voelund::application {
namespace {

using document::SketchElement;
using document::SketchElementId;
using document::SketchGeometry;
using document::SketchLine;
using document::SketchPoint;

SketchPoint interpolate(const SketchLine& line, double parameter) {
    return {line.start.x + (line.end.x - line.start.x) * parameter,
            line.start.y + (line.end.y - line.start.y) * parameter};
}

std::optional<std::pair<double, double>> lineIntersection(const SketchLine& first,
                                                          const SketchLine& second) {
    const double ax = first.end.x - first.start.x;
    const double ay = first.end.y - first.start.y;
    const double bx = second.end.x - second.start.x;
    const double by = second.end.y - second.start.y;
    const double divisor = ax * by - ay * bx;
    if (std::abs(divisor) <= 1.0e-10)
        return std::nullopt;
    const double cx = second.start.x - first.start.x;
    const double cy = second.start.y - first.start.y;
    return std::pair{(cx * by - cy * bx) / divisor, (cx * ay - cy * ax) / divisor};
}

double projectionParameter(const SketchLine& line, SketchPoint point) {
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    return ((point.x - line.start.x) * dx + (point.y - line.start.y) * dy) /
           (dx * dx + dy * dy);
}

SketchPoint reflected(SketchPoint point, const SketchLine& axis) {
    const double parameter = projectionParameter(axis, point);
    const SketchPoint foot = interpolate(axis, parameter);
    return {2.0 * foot.x - point.x, 2.0 * foot.y - point.y};
}

SketchPoint rotated(SketchPoint point, double angle) {
    return {point.x * std::cos(angle) - point.y * std::sin(angle),
            point.x * std::sin(angle) + point.y * std::cos(angle)};
}

std::vector<SketchElementId> selectedIds(
    std::span<const document::SketchGeometryReference> references,
    std::optional<SketchElementId> selected) {
    std::vector<SketchElementId> result;
    for (const auto& reference : references)
        if (std::ranges::find(result, reference.element) == result.end())
            result.push_back(reference.element);
    if (result.empty() && selected.has_value())
        result.push_back(*selected);
    return result;
}

SketchGeometry translatedGeometry(SketchGeometry geometry, double dx, double dy) {
    std::visit(
        [dx, dy](auto& value) {
            using Geometry = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
                value.start.x += dx;
                value.start.y += dy;
                value.end.x += dx;
                value.end.y += dy;
            } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                value.first.x += dx;
                value.first.y += dy;
                value.second.x += dx;
                value.second.y += dy;
            } else {
                value.center.x += dx;
                value.center.y += dy;
            }
        },
        geometry);
    return geometry;
}

SketchGeometry rotatedGeometry(SketchGeometry geometry, double angle) {
    std::visit(
        [angle](auto& value) {
            using Geometry = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
                value.start = rotated(value.start, angle);
                value.end = rotated(value.end, angle);
            } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                const SketchPoint first = rotated(value.first, angle);
                const SketchPoint second = rotated(value.second, angle);
                value.first = first;
                value.second = second;
            } else if constexpr (std::is_same_v<Geometry, document::SketchArc>) {
                value.center = rotated(value.center, angle);
                value.startAngle += angle;
                value.endAngle += angle;
            } else if constexpr (std::is_same_v<Geometry, document::SketchEllipse>) {
                value.center = rotated(value.center, angle);
                value.rotation += angle;
            } else {
                value.center = rotated(value.center, angle);
            }
        },
        geometry);
    return geometry;
}

} // namespace

bool SketchEditorSession::trimSelected() {
    if (!active_ || !selectedElement_.has_value() || !selectionPoint_.has_value())
        return false;
    const auto match = std::ranges::find(elements_.values, *selectedElement_, &SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    const double picked = projectionParameter(*line, *selectionPoint_);
    std::optional<double> cut;
    for (const SketchElement& candidate : elements_.values) {
        const auto* other = std::get_if<SketchLine>(&candidate.geometry);
        if (candidate.id == match->id || other == nullptr)
            continue;
        const auto intersection = lineIntersection(*line, *other);
        if (intersection.has_value() && intersection->first > 1.0e-8 &&
            intersection->first < 1.0 - 1.0e-8 && intersection->second >= -1.0e-8 &&
            intersection->second <= 1.0 + 1.0e-8 &&
            (!cut.has_value() || std::abs(intersection->first - picked) < std::abs(*cut - picked)))
            cut = intersection->first;
    }
    if (!cut.has_value())
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    const bool trimStart = picked < *cut;
    (trimStart ? line->start : line->end) = interpolate(*line, *cut);
    const auto endpoint = trimStart ? document::SketchPointPosition::start
                                    : document::SketchPointPosition::end;
    std::erase_if(constraints_.values, [id = match->id, endpoint](const auto& constraint) {
        const auto affected = [id, endpoint](const auto& reference) {
            return reference.element == id && reference.point == endpoint;
        };
        return affected(constraint.first) ||
               (constraint.second.has_value() && affected(*constraint.second)) ||
               (constraint.third.has_value() && affected(*constraint.third));
    });
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
}

bool SketchEditorSession::extendSelected() {
    if (!active_ || !selectedElement_.has_value() || !selectionPoint_.has_value())
        return false;
    auto match = std::ranges::find(elements_.values, *selectedElement_, &SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    const bool extendStart = projectionParameter(*line, *selectionPoint_) < 0.5;
    std::optional<double> target;
    for (const SketchElement& candidate : elements_.values) {
        const auto* other = std::get_if<SketchLine>(&candidate.geometry);
        if (candidate.id == match->id || other == nullptr)
            continue;
        const auto intersection = lineIntersection(*line, *other);
        if (!intersection.has_value() || intersection->second < -1.0e-8 ||
            intersection->second > 1.0 + 1.0e-8)
            continue;
        const double parameter = intersection->first;
        if (extendStart && parameter < 0.0 && (!target || parameter > *target))
            target = parameter;
        if (!extendStart && parameter > 1.0 && (!target || parameter < *target))
            target = parameter;
    }
    if (!target.has_value())
        return false;
    const auto previous = elements_;
    (extendStart ? line->start : line->end) = interpolate(*line, *target);
    if (solveConstraints())
        return true;
    elements_ = previous;
    return false;
}

bool SketchEditorSession::splitSelected() {
    if (!canAllocateIds(1, 1))
        return false;
    if (!active_ || !selectedElement_.has_value() || !selectionPoint_.has_value())
        return false;
    auto match = std::ranges::find(elements_.values, *selectedElement_, &SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    const double parameter = std::clamp(projectionParameter(*line, *selectionPoint_), 0.0, 1.0);
    if (parameter <= 1.0e-6 || parameter >= 1.0 - 1.0e-6)
        return false;
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    const SketchElementId originalId = match->id;
    const bool construction = match->construction;
    const auto directionConstraint = line->constraint;
    const SketchPoint oldEnd = line->end;
    const SketchPoint split = interpolate(*line, parameter);
    line->end = split;
    const SketchElementId newId{elements_.nextId++};
    elements_.values.push_back(
        {newId, SketchLine{split, oldEnd, directionConstraint}, construction});
    for (auto& constraint : constraints_.values) {
        if (constraint.kind == document::SketchConstraintKind::distance &&
            constraint.first.element == originalId)
            constraint.value *= parameter;
        const auto transferEnd = [originalId, newId](auto& reference) {
            if (reference.element == originalId &&
                reference.point == document::SketchPointPosition::end)
                reference.element = newId;
        };
        transferEnd(constraint.first);
        if (constraint.second.has_value())
            transferEnd(*constraint.second);
        if (constraint.third.has_value())
            transferEnd(*constraint.third);
    }
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                   document::SketchConstraintKind::coincident,
                                   {originalId, document::SketchPointPosition::end},
                                   document::SketchGeometryReference{
                                       newId, document::SketchPointPosition::start}});
    selectedElement_ = newId;
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
}

bool SketchEditorSession::offsetSelected(double distance) {
    if (!canAllocateIds(1, 0))
        return false;
    if (!active_ || !selectedElement_.has_value() || !std::isfinite(distance) || distance == 0.0)
        return false;
    const auto match = std::ranges::find(elements_.values, *selectedElement_, &SketchElement::id);
    if (match == elements_.values.end())
        return false;
    SketchGeometry result = match->geometry;
    if (auto* line = std::get_if<SketchLine>(&result)) {
        const double dx = line->end.x - line->start.x;
        const double dy = line->end.y - line->start.y;
        const double length = std::hypot(dx, dy);
        result = translatedGeometry(result, -dy * distance / length, dx * distance / length);
    } else if (auto* circle = std::get_if<document::SketchCircle>(&result)) {
        circle->radius += distance;
    } else if (auto* arc = std::get_if<document::SketchArc>(&result)) {
        arc->radius += distance;
    } else if (auto* ellipse = std::get_if<document::SketchEllipse>(&result)) {
        ellipse->majorRadius += distance;
        ellipse->minorRadius += distance;
    } else {
        return false;
    }
    const auto previous = elements_;
    const SketchElementId id{elements_.nextId++};
    elements_.values.push_back({id, std::move(result), match->construction});
    selectedElement_ = id;
    try {
        document::validateSketchElements(elements_);
    } catch (const std::invalid_argument&) {
        elements_ = previous;
        return false;
    }
    return true;
}

bool SketchEditorSession::mirrorSelected() {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.size() < 2 || !canAllocateIds(ids.size() - 1, 0))
        return false;
    const auto axisMatch = std::ranges::find(elements_.values, ids.back(), &SketchElement::id);
    if (axisMatch == elements_.values.end())
        return false;
    const auto* axis = std::get_if<SketchLine>(&axisMatch->geometry);
    if (axis == nullptr)
        return false;
    for (std::size_t index = 0; index + 1 < ids.size(); ++index) {
        const auto source = std::ranges::find(elements_.values, ids[index], &SketchElement::id);
        if (source == elements_.values.end())
            continue;
        SketchGeometry geometry = source->geometry;
        std::visit(
            [axis](auto& value) {
                using Geometry = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Geometry, SketchLine>) {
                    value.start = reflected(value.start, *axis);
                    value.end = reflected(value.end, *axis);
                } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                    value.first = reflected(value.first, *axis);
                    value.second = reflected(value.second, *axis);
                } else if constexpr (std::is_same_v<Geometry, document::SketchArc>) {
                    const SketchPoint oldStart{value.center.x + value.radius * std::cos(value.startAngle),
                                               value.center.y + value.radius * std::sin(value.startAngle)};
                    const SketchPoint oldEnd{value.center.x + value.radius * std::cos(value.endAngle),
                                             value.center.y + value.radius * std::sin(value.endAngle)};
                    value.center = reflected(value.center, *axis);
                    const SketchPoint start = reflected(oldEnd, *axis);
                    const SketchPoint end = reflected(oldStart, *axis);
                    value.startAngle = std::atan2(start.y - value.center.y, start.x - value.center.x);
                    value.endAngle = std::atan2(end.y - value.center.y, end.x - value.center.x);
                    while (value.endAngle <= value.startAngle)
                        value.endAngle += 2.0 * std::numbers::pi;
                } else if constexpr (std::is_same_v<Geometry, document::SketchEllipse>) {
                    const SketchPoint major{value.center.x + std::cos(value.rotation),
                                            value.center.y + std::sin(value.rotation)};
                    value.center = reflected(value.center, *axis);
                    const SketchPoint reflectedMajor = reflected(major, *axis);
                    value.rotation = std::atan2(reflectedMajor.y - value.center.y,
                                                reflectedMajor.x - value.center.x);
                } else {
                    value.center = reflected(value.center, *axis);
                }
            },
            geometry);
        elements_.values.push_back(
            {SketchElementId{elements_.nextId++}, std::move(geometry), source->construction});
    }
    return true;
}

bool SketchEditorSession::linearPatternSelected(int count, double spacing) {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.empty() || count < 2 || !std::isfinite(spacing) || spacing == 0.0)
        return false;
    if (!document::hasRepeatedIdCapacity(elements_.nextId, ids.size(),
                                         static_cast<std::uint64_t>(count - 1)))
        return false;
    const auto original = elements_.values;
    for (int copy = 1; copy < count; ++copy)
        for (SketchElementId id : ids) {
            const auto source = std::ranges::find(original, id, &SketchElement::id);
            if (source != original.end())
                elements_.values.push_back({SketchElementId{elements_.nextId++},
                                            translatedGeometry(source->geometry, spacing * copy, 0.0),
                                            source->construction});
        }
    return true;
}

bool SketchEditorSession::circularPatternSelected(int count, double angleDegrees) {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.empty() || count < 2 || !std::isfinite(angleDegrees) ||
        angleDegrees == 0.0)
        return false;
    if (!document::hasRepeatedIdCapacity(elements_.nextId, ids.size(),
                                         static_cast<std::uint64_t>(count - 1)))
        return false;
    const auto original = elements_.values;
    for (int copy = 1; copy < count; ++copy)
        for (SketchElementId id : ids) {
            const auto source = std::ranges::find(original, id, &SketchElement::id);
            if (source != original.end())
                elements_.values.push_back(
                    {SketchElementId{elements_.nextId++},
                     rotatedGeometry(source->geometry,
                                     kernel::toRadians(kernel::Degrees(angleDegrees)).value() *
                                         (static_cast<double>(copy) / count)),
                     source->construction});
        }
    return true;
}

bool SketchEditorSession::filletSelected(double radius) {
    if (!canAllocateIds(1, 4))
        return false;
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.size() != 2 || !std::isfinite(radius) || radius <= 1.0e-9)
        return false;
    auto firstMatch = std::ranges::find(elements_.values, ids[0], &SketchElement::id);
    auto secondMatch = std::ranges::find(elements_.values, ids[1], &SketchElement::id);
    if (firstMatch == elements_.values.end() || secondMatch == elements_.values.end())
        return false;
    auto* first = std::get_if<SketchLine>(&firstMatch->geometry);
    auto* second = std::get_if<SketchLine>(&secondMatch->geometry);
    if (first == nullptr || second == nullptr)
        return false;
    const auto intersection = lineIntersection(*first, *second);
    if (!intersection.has_value())
        return false;
    const SketchPoint vertex = interpolate(*first, intersection->first);
    const auto outward = [vertex](const SketchLine& line) {
        const SketchPoint target = std::hypot(line.start.x - vertex.x, line.start.y - vertex.y) >
                                           std::hypot(line.end.x - vertex.x, line.end.y - vertex.y)
                                       ? line.start
                                       : line.end;
        const double length = std::hypot(target.x - vertex.x, target.y - vertex.y);
        return SketchPoint{(target.x - vertex.x) / length, (target.y - vertex.y) / length};
    };
    const SketchPoint firstDirection = outward(*first);
    const SketchPoint secondDirection = outward(*second);
    const double theta = std::acos(std::clamp(firstDirection.x * secondDirection.x +
                                                  firstDirection.y * secondDirection.y,
                                              -1.0, 1.0));
    if (theta <= 1.0e-6 || theta >= std::numbers::pi - 1.0e-6)
        return false;
    const double tangentDistance = radius / std::tan(theta / 2.0);
    const SketchPoint firstTangent{vertex.x + firstDirection.x * tangentDistance,
                                   vertex.y + firstDirection.y * tangentDistance};
    const SketchPoint secondTangent{vertex.x + secondDirection.x * tangentDistance,
                                    vertex.y + secondDirection.y * tangentDistance};
    const double bisectorLength = std::hypot(firstDirection.x + secondDirection.x,
                                             firstDirection.y + secondDirection.y);
    const double centerDistance = radius / std::sin(theta / 2.0);
    const SketchPoint center{vertex.x + (firstDirection.x + secondDirection.x) /
                                            bisectorLength * centerDistance,
                             vertex.y + (firstDirection.y + secondDirection.y) /
                                            bisectorLength * centerDistance};
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    const auto firstEndpoint = std::hypot(first->start.x - vertex.x, first->start.y - vertex.y) <
                                       std::hypot(first->end.x - vertex.x, first->end.y - vertex.y)
                                   ? document::SketchPointPosition::start
                                   : document::SketchPointPosition::end;
    const auto secondEndpoint = std::hypot(second->start.x - vertex.x, second->start.y - vertex.y) <
                                        std::hypot(second->end.x - vertex.x, second->end.y - vertex.y)
                                    ? document::SketchPointPosition::start
                                    : document::SketchPointPosition::end;
    (firstEndpoint == document::SketchPointPosition::start ? first->start : first->end) =
        firstTangent;
    (secondEndpoint == document::SketchPointPosition::start ? second->start : second->end) =
        secondTangent;
    double startAngle = std::atan2(firstTangent.y - center.y, firstTangent.x - center.x);
    double endAngle = std::atan2(secondTangent.y - center.y, secondTangent.x - center.x);
    double sweep = std::fmod(endAngle - startAngle + 2.0 * std::numbers::pi,
                            2.0 * std::numbers::pi);
    if (sweep > std::numbers::pi) {
        std::swap(startAngle, endAngle);
        sweep = 2.0 * std::numbers::pi - sweep;
    }
    const SketchElementId arcId{elements_.nextId++};
    elements_.values.push_back(
        {arcId, document::SketchArc{center, radius, startAngle, startAngle + sweep}});
    const SketchPoint arcStart{center.x + radius * std::cos(startAngle),
                               center.y + radius * std::sin(startAngle)};
    const bool firstIsArcStart =
        std::hypot(arcStart.x - firstTangent.x, arcStart.y - firstTangent.y) < radius * 1.0e-6;
    const auto addConstraint = [this](document::SketchConstraintKind kind,
                                      document::SketchGeometryReference firstReference,
                                      document::SketchGeometryReference secondReference) {
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++}, kind,
                                       firstReference, secondReference});
    };
    addConstraint(document::SketchConstraintKind::coincident, {ids[0], firstEndpoint},
                  {arcId, firstIsArcStart ? document::SketchPointPosition::start
                                          : document::SketchPointPosition::end});
    addConstraint(document::SketchConstraintKind::coincident, {ids[1], secondEndpoint},
                  {arcId, firstIsArcStart ? document::SketchPointPosition::end
                                          : document::SketchPointPosition::start});
    addConstraint(document::SketchConstraintKind::tangent,
                  {ids[0], document::SketchPointPosition::start},
                  {arcId, document::SketchPointPosition::start});
    addConstraint(document::SketchConstraintKind::tangent,
                  {ids[1], document::SketchPointPosition::start},
                  {arcId, document::SketchPointPosition::start});
    selectedElement_ = arcId;
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
}

} // namespace voelund::application
