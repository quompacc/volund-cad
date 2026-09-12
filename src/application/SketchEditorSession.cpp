#include <voelund/application/SketchEditorSession.hpp>

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchProfiles.hpp>

#include <algorithm>
#include <cmath>
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

} // namespace

SketchEditorSession::SketchEditorSession(CadSession& session, document::FeatureId supportFeature,
                                         document::SketchPlane plane)
    : session_(session), supportFeature_(supportFeature), plane_(plane) {}

SketchEditorSession::SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                                         document::FeatureId supportFeature,
                                         document::SketchParameters parameters)
    : session_(session), supportFeature_(supportFeature), plane_(parameters.plane),
      elements_(std::move(parameters.elements)), constraints_(std::move(parameters.constraints)),
      editingFeature_(sketchFeature) {
    document::validateSketchParameters({plane_, elements_, constraints_});
}

void SketchEditorSession::setTool(SketchDrawingTool tool) noexcept {
    tool_ = tool;
    preview_.reset();
}

SketchDrawingTool SketchEditorSession::tool() const noexcept { return tool_; }

document::SketchGeometry SketchEditorSession::geometry(document::SketchPoint first,
                                                       document::SketchPoint second) const {
    if (tool_ == SketchDrawingTool::line) {
        return document::SketchLine{first, second};
    }
    if (tool_ == SketchDrawingTool::rectangle) {
        return document::SketchRectangle{first.x, first.y, second.x, second.y};
    }
    return document::SketchCircle{first, std::hypot(second.x - first.x, second.y - first.y)};
}

bool SketchEditorSession::setPreview(document::SketchPoint first, document::SketchPoint second) {
    if (!active_) {
        return false;
    }
    const document::SketchGeometry candidate = geometry(first, second);
    document::SketchElements validated = elements_;
    const document::SketchElementId candidateId{validated.nextId++};
    validated.values.push_back({candidateId, candidate});
    try {
        document::validateSketchElements(validated);
    } catch (const std::invalid_argument&) {
        preview_.reset();
        return false;
    }
    preview_ = candidate;
    return true;
}

void SketchEditorSession::clearPreview() noexcept { preview_.reset(); }

std::optional<document::SketchElementId> SketchEditorSession::confirmPreview() {
    if (!active_ || !preview_.has_value()) {
        return std::nullopt;
    }
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&*preview_)) {
        return confirmRectangle(*rectangle);
    }
    const document::SketchElementId id{elements_.nextId++};
    elements_.values.push_back({id, std::move(*preview_)});
    preview_.reset();
    selectedElement_ = id;
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    return id;
}

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
    document::SketchGeometryReference reference{elementId, document::SketchPointPosition::start};
    if (const auto* line = std::get_if<document::SketchLine>(&match->geometry)) {
        const double startDistance =
            std::hypot(position.x - line->start.x, position.y - line->start.y);
        const double endDistance = std::hypot(position.x - line->end.x, position.y - line->end.y);
        reference.point = startDistance <= endDistance ? document::SketchPointPosition::start
                                                       : document::SketchPointPosition::end;
    } else if (std::holds_alternative<document::SketchCircle>(match->geometry)) {
        reference.point = document::SketchPointPosition::center;
    } else {
        return selectElement(elementId);
    }

    if (!additive)
        selectedReferences_.clear();
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
        !std::isfinite(deltaY))
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    const document::SketchElements previousElements = elements_;
    const bool pointSelection =
        selectedReferences_.size() == 1 && selectedReferences_.front().element == *selectedElement_;
    if (pointSelection) {
        const auto reference = selectedReferences_.front();
        if (auto* line = std::get_if<document::SketchLine>(&match->geometry)) {
            document::SketchPoint& point =
                reference.point == document::SketchPointPosition::start ? line->start : line->end;
            point.x += deltaX;
            point.y += deltaY;
        } else if (auto* circle = std::get_if<document::SketchCircle>(&match->geometry)) {
            circle->center.x += deltaX;
            circle->center.y += deltaY;
        }
    } else {
        std::visit(
            [deltaX, deltaY](auto& geometry) {
                using Geometry = std::decay_t<decltype(geometry)>;
                if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
                    geometry.start.x += deltaX;
                    geometry.start.y += deltaY;
                    geometry.end.x += deltaX;
                    geometry.end.y += deltaY;
                } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                    geometry.first.x += deltaX;
                    geometry.first.y += deltaY;
                    geometry.second.x += deltaX;
                    geometry.second.y += deltaY;
                } else {
                    geometry.center.x += deltaX;
                    geometry.center.y += deltaY;
                }
            },
            match->geometry);
    }
    document::validateSketchElements(elements_);
    if (deltaX == 0.0 && deltaY == 0.0)
        return false;
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    return false;
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
    const auto* circle = std::get_if<document::SketchCircle>(&match->geometry);
    return circle == nullptr ? std::nullopt : std::optional<double>{circle->radius};
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
    if (!active_ || !std::isfinite(length) || length <= 1.0e-9 || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return false;
    const document::SketchElements previousElements = elements_;
    const document::SketchConstraints previousConstraints = constraints_;
    const double current = std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
    const double scale = length / current;
    line->end.x = line->start.x + (line->end.x - line->start.x) * scale;
    line->end.y = line->start.y + (line->end.y - line->start.y) * scale;
    const bool constraintChanged = upsertUnaryConstraint(
        constraints_, *selectedElement_, document::SketchConstraintKind::distance, length);
    document::validateSketchElements(elements_);
    if (length == current && !constraintChanged)
        return false;
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
}

bool SketchEditorSession::setSelectedRectangleWidth(double width) {
    if (!active_ || !std::isfinite(width) || width <= 1.0e-9 || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    if (rectangle == nullptr)
        return false;
    const double current = std::abs(rectangle->second.x - rectangle->first.x);
    const double direction = rectangle->second.x < rectangle->first.x ? -1.0 : 1.0;
    rectangle->second.x = rectangle->first.x + direction * width;
    document::validateSketchElements(elements_);
    return width != current;
}

bool SketchEditorSession::setSelectedRectangleHeight(double height) {
    if (!active_ || !std::isfinite(height) || height <= 1.0e-9 || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    if (rectangle == nullptr)
        return false;
    const double current = std::abs(rectangle->second.y - rectangle->first.y);
    const double direction = rectangle->second.y < rectangle->first.y ? -1.0 : 1.0;
    rectangle->second.y = rectangle->first.y + direction * height;
    document::validateSketchElements(elements_);
    return height != current;
}

bool SketchEditorSession::setSelectedCircleRadius(double radius) {
    if (!active_ || !std::isfinite(radius) || radius <= 1.0e-9 || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    auto* circle = std::get_if<document::SketchCircle>(&match->geometry);
    if (circle == nullptr)
        return false;
    const document::SketchElements previousElements = elements_;
    const document::SketchConstraints previousConstraints = constraints_;
    const double current = circle->radius;
    circle->radius = radius;
    const bool constraintChanged = upsertUnaryConstraint(
        constraints_, *selectedElement_, document::SketchConstraintKind::radius, radius);
    document::validateSketchElements(elements_);
    if (radius == current && !constraintChanged)
        return false;
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
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
    const document::SketchElements previousElements = elements_;
    const document::SketchConstraints previousConstraints = constraints_;
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
    document::validateSketchElements(elements_);
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    return false;
}

bool SketchEditorSession::canAddSelectedRelation(
    document::SketchConstraintKind kind) const noexcept {
    if (!active_ || selectedReferences_.size() != 2 ||
        (kind != document::SketchConstraintKind::coincident &&
         kind != document::SketchConstraintKind::parallel &&
         kind != document::SketchConstraintKind::perpendicular &&
         kind != document::SketchConstraintKind::pointOnObject))
        return false;
    const auto& first = selectedReferences_[0];
    const auto& second = selectedReferences_[1];
    if (first == second)
        return false;
    if (kind == document::SketchConstraintKind::pointOnObject) {
        if (first.element == second.element)
            return false;
        const auto sourceElement =
            std::ranges::find(elements_.values, first.element, &document::SketchElement::id);
        const auto targetElement =
            std::ranges::find(elements_.values, second.element, &document::SketchElement::id);
        return sourceElement != elements_.values.end() && targetElement != elements_.values.end() &&
               (std::holds_alternative<document::SketchLine>(sourceElement->geometry) ||
                std::holds_alternative<document::SketchCircle>(sourceElement->geometry)) &&
               (std::holds_alternative<document::SketchLine>(targetElement->geometry) ||
                std::holds_alternative<document::SketchCircle>(targetElement->geometry));
    }
    if (kind != document::SketchConstraintKind::coincident) {
        if (first.element == second.element)
            return false;
        const auto firstElement =
            std::ranges::find(elements_.values, first.element, &document::SketchElement::id);
        const auto secondElement =
            std::ranges::find(elements_.values, second.element, &document::SketchElement::id);
        if (firstElement == elements_.values.end() || secondElement == elements_.values.end() ||
            !std::holds_alternative<document::SketchLine>(firstElement->geometry) ||
            !std::holds_alternative<document::SketchLine>(secondElement->geometry))
            return false;
    }
    return true;
}

bool SketchEditorSession::addSelectedRelation(document::SketchConstraintKind kind) {
    if (!canAddSelectedRelation(kind))
        return false;
    const auto& first = selectedReferences_[0];
    const auto& second = selectedReferences_[1];
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++}, kind, first, second, 0.0});
    document::SketchParameters sketch{plane_, elements_, constraints_};
    lastSolveResult_ = features::solveSketchConstraints(sketch);
    if (lastSolveResult_.status == features::SketchSolveStatus::success)
        elements_ = std::move(sketch.elements);
    return true;
}

bool SketchEditorSession::removeLastConstraint() {
    if (!active_ || constraints_.values.empty())
        return false;
    constraints_.values.pop_back();
    static_cast<void>(solveConstraints());
    return true;
}

bool SketchEditorSession::removeSelected() {
    if (!active_ || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    std::erase_if(constraints_.values, [id = *selectedElement_](const auto& constraint) {
        return constraint.first.element == id ||
               (constraint.second.has_value() && constraint.second->element == id);
    });
    elements_.values.erase(match);
    selectedElement_.reset();
    selectedReferences_.clear();
    preview_.reset();
    if (elements_.values.empty())
        lastSolveResult_ = {features::SketchSolveStatus::success, -1, {}};
    else
        static_cast<void>(solveConstraints());
    return true;
}

std::optional<document::SketchElementId> SketchEditorSession::selectedElement() const noexcept {
    return selectedElement_;
}

std::span<const document::SketchGeometryReference>
SketchEditorSession::selectedReferences() const noexcept {
    return selectedReferences_;
}

bool SketchEditorSession::isElementConflicting(document::SketchElementId elementId) const noexcept {
    for (document::SketchConstraintId id : lastSolveResult_.conflictingConstraints) {
        const auto match =
            std::ranges::find(constraints_.values, id, &document::SketchConstraint::id);
        if (match != constraints_.values.end() &&
            (match->first.element == elementId ||
             (match->second.has_value() && match->second->element == elementId)))
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
    if (includePreview && preview_.has_value()) {
        result.values.push_back({document::SketchElementId{result.nextId}, *preview_});
    }
    return {plane_, std::move(result), constraints_};
}

bool SketchEditorSession::solveConstraints() {
    document::SketchParameters sketch{plane_, elements_, constraints_};
    lastSolveResult_ = features::solveSketchConstraints(sketch);
    if (lastSolveResult_.status != features::SketchSolveStatus::success)
        return false;
    elements_ = std::move(sketch.elements);
    return true;
}

std::vector<features::EvaluatedSketchElement> SketchEditorSession::evaluatedElements() const {
    return features::evaluateSketchElements(parameters(true));
}

std::size_t SketchEditorSession::closedProfileCount() const {
    return features::detectClosedProfiles(elements_.values).size();
}

bool SketchEditorSession::canAccept() const noexcept {
    return active_ && !elements_.values.empty() &&
           lastSolveResult_.status != features::SketchSolveStatus::conflicting;
}

document::FeatureId SketchEditorSession::accept() {
    if (!canAccept()) {
        throw std::logic_error("A sketch editor session requires at least one element");
    }
    preview_.reset();
    const document::SketchParameters result = parameters(false);
    const document::FeatureId id = editingFeature_.has_value()
                                       ? *editingFeature_
                                       : session_.addSketchFeature(supportFeature_, result);
    if (editingFeature_.has_value()) {
        static_cast<void>(session_.setSketchParameters(id, result));
    }
    active_ = false;
    return id;
}

void SketchEditorSession::cancel() noexcept {
    active_ = false;
    preview_.reset();
    selectedElement_.reset();
    selectedReferences_.clear();
}

bool SketchEditorSession::isActive() const noexcept { return active_; }

} // namespace voelund::application
