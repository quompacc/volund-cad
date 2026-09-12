#include <voelund/application/SketchEditorSession.hpp>

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchProfiles.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace voelund::application {

SketchEditorSession::SketchEditorSession(
    CadSession& session, document::FeatureId supportFeature, document::SketchPlane plane)
    : session_(session), supportFeature_(supportFeature), plane_(plane) {}

SketchEditorSession::SketchEditorSession(
    CadSession& session, document::FeatureId sketchFeature,
    document::FeatureId supportFeature, document::SketchParameters parameters)
    : session_(session), supportFeature_(supportFeature), plane_(parameters.plane),
      elements_(std::move(parameters.elements)), editingFeature_(sketchFeature) {
    document::validateSketchParameters({plane_, elements_});
}

void SketchEditorSession::setTool(SketchDrawingTool tool) noexcept {
    tool_ = tool;
    preview_.reset();
}

SketchDrawingTool SketchEditorSession::tool() const noexcept { return tool_; }

document::SketchGeometry SketchEditorSession::geometry(
    document::SketchPoint first, document::SketchPoint second) const {
    if (tool_ == SketchDrawingTool::line) {
        return document::SketchLine{first, second};
    }
    if (tool_ == SketchDrawingTool::rectangle) {
        return document::SketchRectangle{first.x, first.y, second.x, second.y};
    }
    return document::SketchCircle{first, std::hypot(second.x - first.x, second.y - first.y)};
}

bool SketchEditorSession::setPreview(
    document::SketchPoint first, document::SketchPoint second) {
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
    const document::SketchElementId id{elements_.nextId++};
    elements_.values.push_back({id, std::move(*preview_)});
    preview_.reset();
    selectedElement_ = id;
    return id;
}

bool SketchEditorSession::selectElement(document::SketchElementId elementId) noexcept {
    const auto match = std::ranges::find(elements_.values, elementId, &document::SketchElement::id);
    if (match == elements_.values.end()) {
        return false;
    }
    selectedElement_ = elementId;
    return true;
}

bool SketchEditorSession::translateSelected(double deltaX, double deltaY) {
    if (!active_ || !selectedElement_.has_value()
        || !std::isfinite(deltaX) || !std::isfinite(deltaY)) return false;
    const auto match = std::ranges::find(
        elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end()) return false;
    std::visit([deltaX, deltaY](auto& geometry) {
        using Geometry = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
            geometry.start.x += deltaX; geometry.start.y += deltaY;
            geometry.end.x += deltaX; geometry.end.y += deltaY;
        } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
            geometry.first.x += deltaX; geometry.first.y += deltaY;
            geometry.second.x += deltaX; geometry.second.y += deltaY;
        } else {
            geometry.center.x += deltaX; geometry.center.y += deltaY;
        }
    }, match->geometry);
    document::validateSketchElements(elements_);
    return deltaX != 0.0 || deltaY != 0.0;
}

bool SketchEditorSession::removeSelected() {
    if (!active_ || !selectedElement_.has_value()) return false;
    const auto match = std::ranges::find(
        elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end()) return false;
    elements_.values.erase(match);
    selectedElement_.reset();
    preview_.reset();
    return true;
}

std::optional<document::SketchElementId> SketchEditorSession::selectedElement() const noexcept {
    return selectedElement_;
}

std::span<const document::SketchElement> SketchEditorSession::elements() const noexcept {
    return elements_.values;
}

document::SketchParameters SketchEditorSession::parameters(bool includePreview) const {
    document::SketchElements result = elements_;
    if (includePreview && preview_.has_value()) {
        result.values.push_back({document::SketchElementId{result.nextId}, *preview_});
    }
    return {plane_, std::move(result)};
}

std::vector<features::EvaluatedSketchElement> SketchEditorSession::evaluatedElements() const {
    return features::evaluateSketchElements(parameters(true));
}

std::size_t SketchEditorSession::closedProfileCount() const {
    return features::detectClosedProfiles(elements_.values).size();
}

bool SketchEditorSession::canAccept() const noexcept {
    return active_ && !elements_.values.empty();
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
}

bool SketchEditorSession::isActive() const noexcept { return active_; }

} // namespace voelund::application
