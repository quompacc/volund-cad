#include "SketchGeometryTransform.hpp"
#include "SketchOffset.hpp"
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/IdCapacity.hpp>

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

} // namespace

struct SketchEditorSession::Copy final {
    detail::TransformedCopy shape;
    bool construction;
};

// S09: the selection's geometry in selection order, for the offset.
std::vector<SketchGeometry> SketchEditorSession::selectedGeometry() const {
    std::vector<SketchGeometry> result;
    for (const SketchElementId id : selectedIds(selectedReferences_, selectedElement_)) {
        const auto match = std::ranges::find(elements_.values, id, &SketchElement::id);
        if (match != elements_.values.end())
            result.push_back(match->geometry);
    }
    return result;
}

std::optional<std::string> SketchEditorSession::offsetProblem(double distance) const {
    if (!active_)
        return "Versatz: Der Skizzeneditor ist nicht aktiv";
    try {
        static_cast<void>(detail::offsetGeometry(selectedGeometry(), distance));
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return std::nullopt;
}

bool SketchEditorSession::previewOffset(double distance) {
    clearPreview();
    if (!active_)
        return false;
    try {
        preview_ = detail::offsetGeometry(selectedGeometry(), distance).elements;
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

bool SketchEditorSession::offsetSelected(double distance) {
    if (!active_ || !selectedElement_.has_value())
        return false;
    detail::SketchOffsetResult offset;
    try {
        offset = detail::offsetGeometry(selectedGeometry(), distance);
    } catch (const std::invalid_argument&) {
        return false;
    }
    if (!canAllocateIds(offset.elements.size(), offset.joints.size()))
        return false;
    const auto source = std::ranges::find(elements_.values, *selectedElement_, &SketchElement::id);
    const bool construction = source != elements_.values.end() && source->construction;
    Rollback rollback(*this);
    clearPreview();
    // The same geometry the preview showed; joints keep the offset contour closed.
    std::vector<SketchElementId> ids;
    for (auto& geometry : offset.elements) {
        ids.push_back(SketchElementId{elements_.nextId++});
        elements_.values.push_back({ids.back(), std::move(geometry), construction});
    }
    for (const auto& joint : offset.joints)
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                       document::SketchConstraintKind::coincident,
                                       {ids[joint.first], joint.firstPoint},
                                       document::SketchGeometryReference{ids[joint.second],
                                                                         joint.secondPoint}});
    selectedElement_ = ids.front();
    selectedReferences_.clear();
    return keepIfValid(rollback);
}

bool SketchEditorSession::mirrorSelected() {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.size() < 2)
        return false;
    const auto axisMatch = std::ranges::find(elements_.values, ids.back(), &SketchElement::id);
    if (axisMatch == elements_.values.end())
        return false;
    const auto* axisLine = std::get_if<SketchLine>(&axisMatch->geometry);
    if (axisLine == nullptr)
        return false;
    // S08: the axis by value; appending copies may reallocate the element vector.
    const auto transform = detail::PlanarTransform::reflection(*axisLine);
    std::vector<Copy> copies;
    for (std::size_t index = 0; index + 1 < ids.size(); ++index) {
        const auto source = std::ranges::find(elements_.values, ids[index], &SketchElement::id);
        if (source == elements_.values.end())
            return false;
        copies.push_back(
            {detail::transformedCopy(source->geometry, transform), source->construction});
    }
    return appendCopies(copies);
}

bool SketchEditorSession::linearPatternSelected(int count, double spacing) {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.empty() || count < 2 || !std::isfinite(spacing) || spacing == 0.0)
        return false;
    if (!document::hasRepeatedIdCapacity(elements_.nextId, ids.size(),
                                         static_cast<std::uint64_t>(count - 1)))
        return false;
    const auto original = elements_.values;
    Rollback rollback(*this);
    for (int copy = 1; copy < count; ++copy)
        for (SketchElementId id : ids) {
            const auto source = std::ranges::find(original, id, &SketchElement::id);
            if (source != original.end())
                elements_.values.push_back({SketchElementId{elements_.nextId++},
                                            translatedGeometry(source->geometry, spacing * copy, 0.0),
                                            source->construction});
        }
    return keepIfValid(rollback);
}

bool SketchEditorSession::circularPatternSelected(int count, double angleDegrees) {
    const auto ids = selectedIds(selectedReferences_, selectedElement_);
    if (!active_ || ids.empty() || count < 2 || !std::isfinite(angleDegrees) ||
        angleDegrees == 0.0)
        return false;
    if (!document::hasRepeatedIdCapacity(elements_.nextId, ids.size(),
                                         static_cast<std::uint64_t>(count - 1)))
        return false;
    const double total = kernel::toRadians(kernel::Degrees(angleDegrees)).value();
    std::vector<Copy> copies;
    for (int copy = 1; copy < count; ++copy) {
        const auto transform =
            detail::PlanarTransform::rotation(total * (static_cast<double>(copy) / count));
        for (SketchElementId id : ids) {
            const auto source = std::ranges::find(elements_.values, id, &SketchElement::id);
            if (source == elements_.values.end())
                return false;
            copies.push_back(
                {detail::transformedCopy(source->geometry, transform), source->construction});
        }
    }
    return appendCopies(copies);
}

// S08: transformed copies with the constraints that keep a migrated rectangle a rectangle.
bool SketchEditorSession::appendCopies(std::span<const Copy> copies) {
    std::uint64_t elementCount = 0, constraintCount = 0;
    for (const auto& copy : copies) {
        elementCount += copy.shape.elements.size();
        constraintCount += copy.shape.constraints.size();
    }
    if (!canAllocateIds(elementCount, constraintCount))
        return false;
    Rollback rollback(*this);
    for (const auto& copy : copies) {
        std::vector<SketchElementId> ids;
        for (const auto& geometry : copy.shape.elements) {
            ids.push_back(SketchElementId{elements_.nextId++});
            elements_.values.push_back({ids.back(), geometry, copy.construction});
        }
        for (const auto& relation : copy.shape.constraints)
            constraints_.values.push_back(
                {document::SketchConstraintId{constraints_.nextId++},
                 relation.kind,
                 {ids[relation.first], relation.firstPoint},
                 document::SketchGeometryReference{ids[relation.second], relation.secondPoint}});
    }
    return keepIfValid(rollback);
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
    const double theta = std::acos(std::clamp(
        firstDirection.x * secondDirection.x + firstDirection.y * secondDirection.y, -1.0, 1.0));
    if (theta <= 1.0e-6 || theta >= std::numbers::pi - 1.0e-6)
        return false;
    const double tangentDistance = radius / std::tan(theta / 2.0);
    const SketchPoint firstTangent{vertex.x + firstDirection.x * tangentDistance,
                                   vertex.y + firstDirection.y * tangentDistance};
    const SketchPoint secondTangent{vertex.x + secondDirection.x * tangentDistance,
                                    vertex.y + secondDirection.y * tangentDistance};
    const double bisectorLength =
        std::hypot(firstDirection.x + secondDirection.x, firstDirection.y + secondDirection.y);
    const double centerDistance = radius / std::sin(theta / 2.0);
    const SketchPoint center{
        vertex.x + (firstDirection.x + secondDirection.x) / bisectorLength * centerDistance,
        vertex.y + (firstDirection.y + secondDirection.y) / bisectorLength * centerDistance};
    Rollback rollback(*this);
    const auto firstEndpoint = std::hypot(first->start.x - vertex.x, first->start.y - vertex.y) <
                                       std::hypot(first->end.x - vertex.x, first->end.y - vertex.y)
                                   ? document::SketchPointPosition::start
                                   : document::SketchPointPosition::end;
    const auto secondEndpoint =
        std::hypot(second->start.x - vertex.x, second->start.y - vertex.y) <
                std::hypot(second->end.x - vertex.x, second->end.y - vertex.y)
            ? document::SketchPointPosition::start
            : document::SketchPointPosition::end;
    (firstEndpoint == document::SketchPointPosition::start ? first->start : first->end) =
        firstTangent;
    (secondEndpoint == document::SketchPointPosition::start ? second->start : second->end) =
        secondTangent;
    double startAngle = std::atan2(firstTangent.y - center.y, firstTangent.x - center.x);
    double endAngle = std::atan2(secondTangent.y - center.y, secondTangent.x - center.x);
    double sweep =
        std::fmod(endAngle - startAngle + 2.0 * std::numbers::pi, 2.0 * std::numbers::pi);
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
    if (!solveConstraints())
        return false;
    rollback.keep();
    return true;
}

} // namespace voelund::application
