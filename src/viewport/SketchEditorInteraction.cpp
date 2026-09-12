#include "SketchEditorInteraction.hpp"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

namespace voelund::viewport {

void SketchEditorInteraction::begin(
    const PlanarFaceSelection& support, SketchElementTool tool,
    const occ::handle<V3d_View>& view) {
    support_ = support;
    tool_ = tool;
    view_ = view;
    firstPoint_.reset();
    active_ = true;
}

void SketchEditorInteraction::cancel() noexcept {
    view_.Nullify();
    firstPoint_.reset();
    active_ = false;
}

void SketchEditorInteraction::setTool(SketchElementTool tool) noexcept {
    tool_ = tool;
    firstPoint_.reset();
}

void SketchEditorInteraction::setSnapping(bool enabled, double linearStep) noexcept {
    snapping_ = enabled;
    linearStep_ = std::max(0.001, linearStep);
}

bool SketchEditorInteraction::isActive() const noexcept { return active_; }

std::optional<std::pair<double, double>> SketchEditorInteraction::project(int x, int y) const {
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
    const gp_Vec xDirection(
        support_.xDirectionX, support_.xDirectionY, support_.xDirectionZ);
    const gp_Vec yDirection = normal.Crossed(xDirection);
    double localX = offset.Dot(xDirection);
    double localY = offset.Dot(yDirection);
    if (snapping_) {
        localX = std::round(localX / linearStep_) * linearStep_;
        localY = std::round(localY / linearStep_) * linearStep_;
    }
    return std::pair<double, double>{localX, localY};
}

bool SketchEditorInteraction::valid(const SketchElementIntent& intent) const noexcept {
    constexpr double minimumSize = 1.0e-6;
    const double deltaX = intent.secondX - intent.firstX;
    const double deltaY = intent.secondY - intent.firstY;
    if (tool_ == SketchElementTool::rectangle) {
        return std::abs(deltaX) >= minimumSize && std::abs(deltaY) >= minimumSize;
    }
    return std::hypot(deltaX, deltaY) >= minimumSize;
}

std::optional<SketchElementIntent> SketchEditorInteraction::move(int x, int y) const {
    if (!firstPoint_.has_value()) {
        return std::nullopt;
    }
    const auto point = project(x, y);
    if (!point.has_value()) {
        return std::nullopt;
    }
    const SketchElementIntent intent{
        firstPoint_->first, firstPoint_->second, point->first, point->second};
    return valid(intent) ? std::optional{intent} : std::nullopt;
}

std::optional<SketchElementIntent> SketchEditorInteraction::click(int x, int y) {
    const auto point = project(x, y);
    if (!point.has_value()) {
        return std::nullopt;
    }
    if (!firstPoint_.has_value()) {
        firstPoint_ = point;
        return std::nullopt;
    }
    const SketchElementIntent intent{
        firstPoint_->first, firstPoint_->second, point->first, point->second};
    if (!valid(intent)) {
        return std::nullopt;
    }
    firstPoint_.reset();
    return intent;
}

} // namespace voelund::viewport
