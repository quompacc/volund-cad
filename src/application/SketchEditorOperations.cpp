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
            } else if constexpr (std::is_same_v<Geometry, document::SketchPointGeometry>) {
                value.position.x += dx;
                value.position.y += dy;
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

// S12: one-time copies, only for selections with a legacy rectangle (no pattern source).
bool SketchEditorSession::copyMirrored() {
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

bool SketchEditorSession::copyLinear(int count, double spacing) {
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

bool SketchEditorSession::copyCircular(int count, double angleDegrees) {
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

} // namespace voelund::application
