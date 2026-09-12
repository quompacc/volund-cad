#include "SketchEditorInteraction.hpp"
#include "SketchViewOrientation.hpp"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

namespace voelund::viewport {

void SketchEditorInteraction::begin(const PlanarFaceSelection& support, SketchElementTool tool,
                                    const occ::handle<V3d_View>& view) {
    support_ = support;
    tool_ = tool;
    view_ = view;
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    active_ = true;
    orientSketchView(support_, view_);
}

void SketchEditorInteraction::cancel() noexcept {
    view_.Nullify();
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    snapPoints_.clear();
    dragPoint_.reset();
    active_ = false;
}

void SketchEditorInteraction::setTool(SketchElementTool tool) noexcept {
    tool_ = tool;
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    dragPoint_.reset();
}

void SketchEditorInteraction::setSnapping(bool enabled, double linearStep) noexcept {
    snapping_ = enabled;
    linearStep_ = std::max(0.001, linearStep);
}

void SketchEditorInteraction::setSnapPoints(std::span<const SceneSketchPoint> points) {
    snapPoints_.assign(points.begin(), points.end());
}

bool SketchEditorInteraction::isActive() const noexcept { return active_; }

bool SketchEditorInteraction::isDrawing() const noexcept {
    return active_ && tool_ != SketchElementTool::select;
}

std::optional<std::pair<double, double>> SketchEditorInteraction::projectPoint(int x, int y) const {
    if (!active_ || view_.IsNull()) {
        return std::nullopt;
    }
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    view_->ConvertWithProj(x, y, px, py, pz, dx, dy, dz);
    const gp_Pnt rayPoint(px, py, pz);
    const gp_Vec rayDirection(dx, dy, dz);
    const gp_Pnt origin(support_.originX, support_.originY, support_.originZ);
    const gp_Vec normal(support_.normalX, support_.normalY, support_.normalZ);
    const double denominator = normal.Dot(rayDirection);
    if (std::abs(denominator) < 1.0e-12) {
        return std::nullopt;
    }
    const double parameter = normal.Dot(gp_Vec(rayPoint, origin)) / denominator;
    const gp_Vec offset(origin, rayPoint.Translated(rayDirection * parameter));
    const gp_Vec xDirection(support_.xDirectionX, support_.xDirectionY, support_.xDirectionZ);
    const gp_Vec yDirection = normal.Crossed(xDirection);
    double localX = offset.Dot(xDirection);
    double localY = offset.Dot(yDirection);
    if (snapping_) {
        localX = std::round(localX / linearStep_) * linearStep_;
        localY = std::round(localY / linearStep_) * linearStep_;
    }
    return std::pair<double, double>{localX, localY};
}

std::optional<SketchEditorInteraction::InteractionPoint>
SketchEditorInteraction::interactionPoint(int x, int y) const {
    const auto projected = projectPoint(x, y);
    if (!projected.has_value())
        return std::nullopt;
    constexpr int snapRadiusPixels = 12;
    const SceneSketchPoint* nearest = nullptr;
    int nearestDistance = snapRadiusPixels * snapRadiusPixels + 1;
    for (const SceneSketchPoint& candidate : snapPoints_) {
        int pointX = 0;
        int pointY = 0;
        view_->Convert(candidate.worldX, candidate.worldY, candidate.worldZ, pointX, pointY);
        const int deltaX = pointX - x;
        const int deltaY = pointY - y;
        const int distance = deltaX * deltaX + deltaY * deltaY;
        if (distance < nearestDistance) {
            nearest = &candidate;
            nearestDistance = distance;
        }
    }
    if (nearest == nullptr)
        return InteractionPoint{*projected, std::nullopt};
    return InteractionPoint{{nearest->localX, nearest->localY},
                            SketchPointReferenceIntent{nearest->elementId, nearest->kind}};
}

bool SketchEditorInteraction::valid(const SketchElementIntent& intent) const noexcept {
    constexpr double minimumSize = 1.0e-6;
    const double deltaX = intent.secondX - intent.firstX;
    const double deltaY = intent.secondY - intent.firstY;
    if (tool_ == SketchElementTool::rectangle) {
        return std::abs(deltaX) >= minimumSize && std::abs(deltaY) >= minimumSize;
    }
    if (tool_ == SketchElementTool::arc && intent.third.has_value()) {
        const double radius = std::hypot(deltaX, deltaY);
        const double endRadius =
            std::hypot(intent.third->first - intent.firstX, intent.third->second - intent.firstY);
        const double cross = deltaX * (intent.third->second - intent.firstY) -
                             deltaY * (intent.third->first - intent.firstX);
        return radius >= minimumSize && endRadius >= minimumSize &&
               std::abs(cross) >= minimumSize * radius * endRadius;
    }
    return std::hypot(deltaX, deltaY) >= minimumSize;
}

std::optional<SketchElementIntent> SketchEditorInteraction::move(int x, int y) const {
    if (!firstPoint_.has_value()) {
        return std::nullopt;
    }
    const auto resolved = interactionPoint(x, y);
    if (!resolved.has_value())
        return std::nullopt;
    const auto& [point, reference] = *resolved;
    const SketchElementIntent intent{firstPoint_->first,
                                     firstPoint_->second,
                                     secondPoint_.has_value() ? secondPoint_->first : point.first,
                                     secondPoint_.has_value() ? secondPoint_->second : point.second,
                                     secondPoint_.has_value() ? std::optional{point} : std::nullopt,
                                     firstReference_,
                                     secondReference_,
                                     secondPoint_.has_value() ? reference : std::nullopt};
    return valid(intent) ? std::optional{intent} : std::nullopt;
}

std::optional<SketchElementIntent> SketchEditorInteraction::click(int x, int y) {
    const auto resolved = interactionPoint(x, y);
    if (!resolved.has_value())
        return std::nullopt;
    const auto& [point, reference] = *resolved;
    if (!firstPoint_.has_value()) {
        firstPoint_ = point;
        firstReference_ = reference;
        return std::nullopt;
    }
    if (tool_ == SketchElementTool::arc && !secondPoint_.has_value()) {
        const SketchElementIntent radiusIntent{firstPoint_->first, firstPoint_->second, point.first,
                                               point.second, std::nullopt, std::nullopt,
                                               std::nullopt, std::nullopt};
        if (!valid(radiusIntent))
            return std::nullopt;
        secondPoint_ = point;
        secondReference_ = reference;
        return std::nullopt;
    }
    const SketchElementIntent intent{firstPoint_->first,
                                     firstPoint_->second,
                                     secondPoint_.has_value() ? secondPoint_->first : point.first,
                                     secondPoint_.has_value() ? secondPoint_->second : point.second,
                                     secondPoint_.has_value() ? std::optional{point} : std::nullopt,
                                     firstReference_,
                                     secondReference_,
                                     secondPoint_.has_value() ? reference : std::nullopt};
    if (!valid(intent)) {
        return std::nullopt;
    }
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    return intent;
}

bool SketchEditorInteraction::beginElementDrag(int x, int y) {
    dragPoint_ = projectPoint(x, y);
    return dragPoint_.has_value();
}

std::optional<std::pair<double, double>> SketchEditorInteraction::dragElement(int x, int y) {
    if (!dragPoint_.has_value())
        return std::nullopt;
    const auto point = projectPoint(x, y);
    if (!point.has_value())
        return std::nullopt;
    const std::pair delta{point->first - dragPoint_->first, point->second - dragPoint_->second};
    dragPoint_ = point;
    if (delta.first == 0.0 && delta.second == 0.0)
        return std::nullopt;
    return delta;
}

void SketchEditorInteraction::finishElementDrag() noexcept { dragPoint_.reset(); }

bool SketchEditorInteraction::isElementDragging() const noexcept { return dragPoint_.has_value(); }

} // namespace voelund::viewport
