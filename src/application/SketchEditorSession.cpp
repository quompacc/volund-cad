#include "SketchGeometryTransform.hpp"
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <voelund/application/CadSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchProfiles.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace voelund::application {
namespace {

bool upsertUnaryConstraint(document::SketchConstraints& constraints,
                           document::SketchElementId element, document::SketchConstraintKind kind,
                           double value = 0.0) {
    const auto match = std::ranges::find_if(constraints.values, [element, kind](const auto& item) {
        return item.kind == kind && item.first.element == element && !item.second.has_value();
    });
    if (match != constraints.values.end()) {
        if (match->value == value)
            return false;
        match->value = value;
        return true;
    }
    constraints.values.push_back(
        {document::SketchConstraintId{constraints.nextId++}, kind,
         document::SketchGeometryReference{element, document::SketchPointPosition::start},
         std::nullopt, value});
    return true;
}

bool valid(const document::SketchElements& elements) {
    try {
        document::validateSketchElements(elements);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace

SketchEditorSession::SketchEditorSession(CadSession& session, document::FeatureId supportFeature,
                                         document::SketchPlane plane,
                                         std::optional<document::SketchSupportBinding> binding)
    : session_(session), supportFeature_(supportFeature), plane_(plane), supportBinding_(binding) {
    if (supportBinding_)
        supportStamp_ = session.sceneStatus().document;
    document::validateCanonicalSketchPlane(plane_);
    commitHistory();
}

SketchEditorSession::SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                                         document::FeatureId supportFeature,
                                         document::SketchParameters parameters)
    : session_(session), supportFeature_(supportFeature), plane_(parameters.plane),
      supportBinding_(parameters.supportBinding), elements_(std::move(parameters.elements)),
      constraints_(std::move(parameters.constraints)),
      patterns_(std::move(parameters.patterns)), editingFeature_(sketchFeature) {
    if (parameters.repairReason)
        throw std::invalid_argument(
            "MigrationNeedsRepair: repair sketch references before editing");
    if (supportBinding_)
        supportStamp_ = session.sceneStatus().document;
    editedBaseline_ = storedSketch(session, sketchFeature);
    document::validateSketchParameters(this->parameters(false));
    static_cast<void>(solveConstraints());
    commitHistory();
}

SketchEditorSession::SketchEditorSession(CadSession& session, document::DatumPlane datum)
    : session_(session), datum_(datum), plane_(document::datumSketchPlane(datum)) {
    commitHistory();
}

SketchEditorSession::SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                                         document::SketchParameters parameters)
    : session_(session), datum_(parameters.datum), plane_(parameters.plane),
      elements_(std::move(parameters.elements)), constraints_(std::move(parameters.constraints)),
      patterns_(std::move(parameters.patterns)), editingFeature_(sketchFeature) {
    if (!datum_)
        throw std::invalid_argument("Only base-plane sketches are edited without a support");
    editedBaseline_ = storedSketch(session, sketchFeature);
    document::validateSketchParameters(this->parameters(false));
    static_cast<void>(solveConstraints());
    commitHistory();
}

void SketchEditorSession::setTool(SketchDrawingTool tool) noexcept {
    tool_ = tool;
    clearPreview();
}

SketchDrawingTool SketchEditorSession::tool() const noexcept { return tool_; }

bool SketchEditorSession::selectElement(document::SketchElementId elementId) noexcept {
    const auto match = std::ranges::find(elements_.values, elementId, &document::SketchElement::id);
    if (match == elements_.values.end()) {
        return false;
    }
    selectedElement_ = elementId;
    selectedReferences_.clear();
    return true;
}

bool SketchEditorSession::selectNearestReference(document::SketchElementId elementId,
                                                 document::SketchPoint position,
                                                 bool additive) noexcept {
    const auto match = std::ranges::find(elements_.values, elementId, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    selectionPoint_ = position;
    document::SketchGeometryReference reference{elementId, document::SketchPointPosition::start};
    if (const auto* line = std::get_if<document::SketchLine>(&match->geometry)) {
        const double startDistance =
            std::hypot(position.x - line->start.x, position.y - line->start.y);
        const double endDistance = std::hypot(position.x - line->end.x, position.y - line->end.y);
        reference.point = startDistance <= endDistance ? document::SketchPointPosition::start
                                                       : document::SketchPointPosition::end;
    } else if (std::holds_alternative<document::SketchCircle>(match->geometry) ||
               std::holds_alternative<document::SketchEllipse>(match->geometry)) {
        reference.point = document::SketchPointPosition::center;
    } else if (const auto* arc = std::get_if<document::SketchArc>(&match->geometry)) {
        const document::SketchPoint start{arc->center.x + arc->radius * std::cos(arc->startAngle),
                                          arc->center.y + arc->radius * std::sin(arc->startAngle)};
        const document::SketchPoint end{arc->center.x + arc->radius * std::cos(arc->endAngle),
                                        arc->center.y + arc->radius * std::sin(arc->endAngle)};
        const double centerDistance =
            std::hypot(position.x - arc->center.x, position.y - arc->center.y);
        const double startDistance = std::hypot(position.x - start.x, position.y - start.y);
        const double endDistance = std::hypot(position.x - end.x, position.y - end.y);
        reference.point = centerDistance <= startDistance && centerDistance <= endDistance
                              ? document::SketchPointPosition::center
                          : startDistance <= endDistance ? document::SketchPointPosition::start
                                                         : document::SketchPointPosition::end;
    } else if (!additive) {
        return selectElement(elementId);
    } // S08: a rectangle joins a multiple selection as a whole element.

    if (!additive)
        selectedReferences_.clear();
    // S08: an element selected as a whole stays in the selection the next pick extends.
    if (additive && selectedReferences_.empty() && selectedElement_.has_value() &&
        *selectedElement_ != elementId)
        selectedReferences_.push_back({*selectedElement_, document::SketchPointPosition::start});
    const auto selected = std::ranges::find(selectedReferences_, reference);
    if (additive && selected != selectedReferences_.end()) {
        selectedReferences_.erase(selected);
        selectedElement_ = selectedReferences_.empty()
                               ? std::nullopt
                               : std::optional{selectedReferences_.back().element};
        return true;
    }
    if (selected == selectedReferences_.end())
        selectedReferences_.push_back(reference);
    selectedElement_ = elementId;
    return true;
}

bool SketchEditorSession::translateSelected(double deltaX, double deltaY) {
    if (!active_ || !selectedElement_.has_value() || !std::isfinite(deltaX) ||
        !std::isfinite(deltaY) || patternLock(false)) // S12: copies follow their pattern
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    Rollback rollback(*this);
    const bool pointSelection =
        selectedReferences_.size() == 1 && selectedReferences_.front().element == *selectedElement_;
    if (pointSelection) {
        // S08: every variant a point reference can name moves; anything else is refused.
        const auto reference = selectedReferences_.front();
        if (auto* line = std::get_if<document::SketchLine>(&match->geometry)) {
            document::SketchPoint& point =
                reference.point == document::SketchPointPosition::start ? line->start : line->end;
            point.x += deltaX;
            point.y += deltaY;
        } else if (auto* circle = std::get_if<document::SketchCircle>(&match->geometry)) {
            circle->center.x += deltaX;
            circle->center.y += deltaY;
        } else if (auto* arc = std::get_if<document::SketchArc>(&match->geometry)) {
            if (reference.point == document::SketchPointPosition::center) {
                arc->center.x += deltaX;
                arc->center.y += deltaY;
            } else {
                double& angle = reference.point == document::SketchPointPosition::start
                                    ? arc->startAngle
                                    : arc->endAngle;
                const double x = arc->center.x + arc->radius * std::cos(angle) + deltaX;
                const double y = arc->center.y + arc->radius * std::sin(angle) + deltaY;
                angle = std::atan2(y - arc->center.y, x - arc->center.x);
                if (reference.point == document::SketchPointPosition::end) {
                    while (angle <= arc->startAngle)
                        angle += 2.0 * std::numbers::pi;
                } else {
                    while (arc->endAngle <= angle)
                        angle -= 2.0 * std::numbers::pi;
                }
            }
        } else if (auto* ellipse = std::get_if<document::SketchEllipse>(&match->geometry)) {
            ellipse->center.x += deltaX;
            ellipse->center.y += deltaY;
        } else {
            return false;
        }
    } else {
        // S08: a selection over several elements moves all of them as a whole.
        std::vector<document::SketchElementId> ids{*selectedElement_};
        for (const auto& reference : selectedReferences_)
            if (std::ranges::find(ids, reference.element) == ids.end())
                ids.push_back(reference.element);
        for (const auto id : ids) {
            const auto element =
                std::ranges::find(elements_.values, id, &document::SketchElement::id);
            if (element == elements_.values.end())
                return false;
            // A translation keeps the axes, so every variant stays itself.
            element->geometry =
                detail::transformedCopy(element->geometry,
                                        detail::PlanarTransform::translation(deltaX, deltaY))
                    .elements.front();
        }
    }
    if ((deltaX == 0.0 && deltaY == 0.0) || !solveConstraints())
        return false;
    rollback.keep();
    return true;
}

std::optional<double> SketchEditorSession::selectedLineLength() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return std::nullopt;
    return std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
}

std::optional<double> SketchEditorSession::selectedRectangleWidth() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    return rectangle == nullptr
               ? std::nullopt
               : std::optional<double>{std::abs(rectangle->second.x - rectangle->first.x)};
}

std::optional<double> SketchEditorSession::selectedRectangleHeight() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    return rectangle == nullptr
               ? std::nullopt
               : std::optional<double>{std::abs(rectangle->second.y - rectangle->first.y)};
}

std::optional<double> SketchEditorSession::selectedCircleRadius() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    if (const auto* circle = std::get_if<document::SketchCircle>(&match->geometry))
        return circle->radius;
    const auto* arc = std::get_if<document::SketchArc>(&match->geometry);
    return arc == nullptr ? std::nullopt : std::optional<double>{arc->radius};
}

std::optional<document::SketchLineConstraint>
SketchEditorSession::selectedLineConstraint() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return std::nullopt;
    for (const document::SketchConstraint& constraint : constraints_.values) {
        if (constraint.first.element != *selectedElement_)
            continue;
        if (constraint.kind == document::SketchConstraintKind::horizontal)
            return document::SketchLineConstraint::horizontal;
        if (constraint.kind == document::SketchConstraintKind::vertical)
            return document::SketchLineConstraint::vertical;
    }
    return line->constraint;
}

bool SketchEditorSession::setSelectedLineLength(double length) {
    if (!active_ || !kernel::modelLength(length) || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    if (!canUpsertUnaryConstraint(*selectedElement_, document::SketchConstraintKind::distance))
        return false;
    Rollback rollback(*this);
    const double current = std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
    const double scale = length / current;
    line->end.x = line->start.x + (line->end.x - line->start.x) * scale;
    line->end.y = line->start.y + (line->end.y - line->start.y) * scale;
    const bool constraintChanged = upsertUnaryConstraint(
        constraints_, *selectedElement_, document::SketchConstraintKind::distance, length);
    if ((length == current && !constraintChanged) || !solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::setSelectedRectangleWidth(double width) {
    if (!active_ || !kernel::modelLength(width) || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    if (rectangle == nullptr)
        return false;
    Rollback rollback(*this);
    const double current = std::abs(rectangle->second.x - rectangle->first.x);
    const double direction = rectangle->second.x < rectangle->first.x ? -1.0 : 1.0;
    rectangle->second.x = rectangle->first.x + direction * width;
    if (!valid(elements_) || width == current || !solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::setSelectedRectangleHeight(double height) {
    if (!active_ || !kernel::modelLength(height) || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    if (rectangle == nullptr)
        return false;
    Rollback rollback(*this);
    const double current = std::abs(rectangle->second.y - rectangle->first.y);
    const double direction = rectangle->second.y < rectangle->first.y ? -1.0 : 1.0;
    rectangle->second.y = rectangle->first.y + direction * height;
    if (!valid(elements_) || height == current || !solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::setSelectedCircleRadius(double radius) {
    if (!active_ || !kernel::modelLength(radius) || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* circle = std::get_if<document::SketchCircle>(&match->geometry);
    auto* arc = std::get_if<document::SketchArc>(&match->geometry);
    if (circle == nullptr && arc == nullptr)
        return false;
    if (!canUpsertUnaryConstraint(*selectedElement_, document::SketchConstraintKind::radius))
        return false;
    Rollback rollback(*this);
    const double current = circle ? circle->radius : arc->radius;
    (circle ? circle->radius : arc->radius) = radius;
    const bool constraintChanged = upsertUnaryConstraint(
        constraints_, *selectedElement_, document::SketchConstraintKind::radius, radius);
    if ((radius == current && !constraintChanged) || !solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::setSelectedLineConstraint(document::SketchLineConstraint constraint) {
    if (!active_ || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    std::size_t directionConstraintCount = 0;
    bool hasRequestedConstraint = false;
    for (const auto& item : constraints_.values) {
        if (item.first.element != *selectedElement_ ||
            (item.kind != document::SketchConstraintKind::horizontal &&
             item.kind != document::SketchConstraintKind::vertical))
            continue;
        ++directionConstraintCount;
        hasRequestedConstraint = hasRequestedConstraint ||
                                 (constraint == document::SketchLineConstraint::horizontal &&
                                  item.kind == document::SketchConstraintKind::horizontal) ||
                                 (constraint == document::SketchLineConstraint::vertical &&
                                  item.kind == document::SketchConstraintKind::vertical);
    }
    if (line->constraint == constraint &&
        ((constraint == document::SketchLineConstraint::none && directionConstraintCount == 0) ||
         (constraint != document::SketchLineConstraint::none && directionConstraintCount == 1 &&
          hasRequestedConstraint)))
        return false;
    if (constraint != document::SketchLineConstraint::none && !canAllocateIds(0, 1))
        return false;
    Rollback rollback(*this);
    std::erase_if(constraints_.values, [id = *selectedElement_](const auto& item) {
        return item.first.element == id &&
               (item.kind == document::SketchConstraintKind::horizontal ||
                item.kind == document::SketchConstraintKind::vertical);
    });
    const double length = std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
    if (constraint == document::SketchLineConstraint::horizontal) {
        const double direction = line->end.x < line->start.x ? -1.0 : 1.0;
        line->end = {line->start.x + direction * length, line->start.y};
    } else if (constraint == document::SketchLineConstraint::vertical) {
        const double direction = line->end.y < line->start.y ? -1.0 : 1.0;
        line->end = {line->start.x, line->start.y + direction * length};
    }
    line->constraint = constraint;
    if (constraint == document::SketchLineConstraint::horizontal)
        static_cast<void>(upsertUnaryConstraint(constraints_, *selectedElement_,
                                                document::SketchConstraintKind::horizontal));
    else if (constraint == document::SketchLineConstraint::vertical)
        static_cast<void>(upsertUnaryConstraint(constraints_, *selectedElement_,
                                                document::SketchConstraintKind::vertical));
    if (!solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::removeLastConstraint() {
    if (!active_ || constraints_.values.empty())
        return false;
    constraints_.values.pop_back();
    static_cast<void>(solveConstraints());
    return true;
}

bool SketchEditorSession::removeConstraint(document::SketchConstraintId id) {
    if (!active_)
        return false;
    const auto match = std::ranges::find(constraints_.values, id, &document::SketchConstraint::id);
    if (match == constraints_.values.end())
        return false;
    constraints_.values.erase(match);
    static_cast<void>(solveConstraints());
    return true;
}

bool SketchEditorSession::removeSelected() {
    if (!active_ || !selectedElement_.has_value() || patternLock(true))
        return false;
    // S08: the whole selection, with every constraint that names one of its elements.
    std::vector<document::SketchElementId> ids{*selectedElement_};
    for (const auto& reference : selectedReferences_)
        if (std::ranges::find(ids, reference.element) == ids.end())
            ids.push_back(reference.element);
    for (const auto id : ids)
        if (std::ranges::find(elements_.values, id, &document::SketchElement::id) ==
            elements_.values.end())
            return false;
    const auto named = [&ids](document::SketchElementId id) {
        return std::ranges::find(ids, id) != ids.end();
    };
    std::erase_if(constraints_.values, [&named](const auto& constraint) {
        return named(constraint.first.element) ||
               (constraint.second.has_value() && named(constraint.second->element)) ||
               (constraint.third.has_value() && named(constraint.third->element));
    });
    std::erase_if(elements_.values,
                  [&named](const document::SketchElement& element) { return named(element.id); });
    selectedElement_.reset();
    selectedReferences_.clear();
    clearPreview();
    refreshSolve();
    return true;
}

std::optional<document::SketchElementId> SketchEditorSession::selectedElement() const noexcept {
    return selectedElement_;
}

std::span<const document::SketchGeometryReference>
SketchEditorSession::selectedReferences() const noexcept {
    return selectedReferences_;
}

bool SketchEditorSession::isElementDetermined(document::SketchElementId elementId) const noexcept {
    return lastSolveResult_.status == features::SketchSolveStatus::success &&
           std::ranges::find(lastSolveResult_.freeElements, elementId) ==
               lastSolveResult_.freeElements.end();
}

bool SketchEditorSession::isElementConflicting(document::SketchElementId elementId) const noexcept {
    for (document::SketchConstraintId id : lastSolveResult_.conflictingConstraints) {
        const auto match =
            std::ranges::find(constraints_.values, id, &document::SketchConstraint::id);
        if (match != constraints_.values.end() &&
            (match->first.element == elementId ||
             (match->second.has_value() && match->second->element == elementId) ||
             (match->third.has_value() && match->third->element == elementId)))
            return true;
    }
    return false;
}

std::span<const document::SketchElement> SketchEditorSession::elements() const noexcept {
    return elements_.values;
}

std::span<const document::SketchConstraint> SketchEditorSession::constraints() const noexcept {
    return constraints_.values;
}

const features::SketchSolveResult& SketchEditorSession::lastSolveResult() const noexcept {
    return lastSolveResult_;
}

document::SketchParameters SketchEditorSession::parameters(bool includePreview) const {
    document::SketchElements result = elements_;
    if (includePreview && canAllocatePreviewIds(preview_))
        for (const document::SketchGeometry& geometry : preview_)
            result.values.push_back({document::SketchElementId{result.nextId++}, geometry});
    document::SketchParameters sketch{plane_, std::move(result), constraints_};
    sketch.supportBinding = supportBinding_;
    sketch.datum = datum_;
    sketch.patterns = patterns_;
    return sketch;
}

std::vector<features::EvaluatedSketchElement> SketchEditorSession::evaluatedElements() const {
    return features::evaluateSketchElements(parameters(true));
}

std::size_t SketchEditorSession::closedProfileCount() const {
    return features::detectClosedProfiles(elements_.values).size();
}

bool SketchEditorSession::canAccept() const noexcept {
    if (!active_ || elements_.values.empty() || dragStart_)
        return false;
    switch (lastSolveResult_.status) {
    case features::SketchSolveStatus::success:
        return lastSolveResult_.degreesOfFreedom >= 0;
    case features::SketchSolveStatus::conflicting:
    case features::SketchSolveStatus::failed:
        return false;
    }
    return false;
}

void SketchEditorSession::cancel() noexcept {
    active_ = false;
    dragStart_.reset();
    clearPreview();
    selectedElement_.reset();
    selectedReferences_.clear();
    selectionPoint_.reset();
}

bool SketchEditorSession::isActive() const noexcept { return active_; }

} // namespace voelund::application
