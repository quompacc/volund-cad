// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorInteraction.hpp"
#include "SketchViewOrientation.hpp"
#include <voelund/kernel/NumericPolicy.hpp>

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace voelund::viewport {
namespace {

bool needsThirdPoint(SketchElementTool tool) {
    return tool == SketchElementTool::arc || tool == SketchElementTool::threePointArc ||
           tool == SketchElementTool::ellipse || tool == SketchElementTool::slot;
}

std::pair<std::pair<double, double>, std::optional<SketchAutomaticConstraint>>
constrainedLineEnd(SketchElementTool tool, const std::pair<double, double>& first,
                   std::pair<double, double> second) {
    if (tool != SketchElementTool::line && tool != SketchElementTool::polyline)
        return {second, std::nullopt};
    const double dx = second.first - first.first;
    const double dy = second.second - first.second;
    const double length = std::hypot(dx, dy);
    if (length <= 1.0e-9)
        return {second, std::nullopt};
    if (std::abs(dy) / length <= 0.08) {
        second.second = first.second;
        return {second, SketchAutomaticConstraint::horizontal};
    }
    if (std::abs(dx) / length <= 0.08) {
        second.first = first.first;
        return {second, SketchAutomaticConstraint::vertical};
    }
    return {second, std::nullopt};
}

} // namespace

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
    dynamic_.reset();
    active_ = false;
}

void SketchEditorInteraction::setTool(SketchElementTool tool) noexcept {
    tool_ = tool;
    continued_.reset();
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    dragPoint_.reset();
    dynamic_.reset();
}

void SketchEditorInteraction::continuePolyline(double x, double y, std::uint64_t elementId) {
    if (active_ && tool_ == SketchElementTool::polyline) {
        firstPoint_ = std::pair{x, y};
        firstReference_ = SketchPointReferenceIntent{elementId, SketchPointKind::end};
        continued_ = elementId;
        dynamic_.begin(tool_); // D01: the next segment's fields
    }
}

void SketchEditorInteraction::setSnapping(bool enabled, double linearStep) noexcept {
    snapping_ = enabled;
    linearStep_ = std::max(0.001, linearStep);
}

void SketchEditorInteraction::setSnapPoints(std::span<const SceneSketchPoint> points) {
    snapPoints_.assign(points.begin(), points.end());
    if (active_) {
        snapPoints_.push_back({0, SketchPointKind::origin, 0.0, 0.0, support_.originX,
                               support_.originY, support_.originZ});
        if (projectedVisible_)
            snapPoints_.insert(snapPoints_.end(), support_.referencePoints.begin(),
                               support_.referencePoints.end());
        for (const PlanarReferenceSegment& segment : support_.referenceSegments) {
            if (support_.stableReferences || !projectedVisible_)
                break;
            snapPoints_.push_back({0, SketchPointKind::projected, segment.firstX, segment.firstY,
                                   segment.firstWorldX, segment.firstWorldY, segment.firstWorldZ,
                                   segment.referenceId, 0.0});
            snapPoints_.push_back({0, SketchPointKind::projected, segment.secondX, segment.secondY,
                                   segment.secondWorldX, segment.secondWorldY, segment.secondWorldZ,
                                   segment.referenceId, 1.0});
            snapPoints_.push_back(
                {0, SketchPointKind::projected, (segment.firstX + segment.secondX) * 0.5,
                 (segment.firstY + segment.secondY) * 0.5,
                 (segment.firstWorldX + segment.secondWorldX) * 0.5,
                 (segment.firstWorldY + segment.secondWorldY) * 0.5,
                 (segment.firstWorldZ + segment.secondWorldZ) * 0.5, segment.referenceId, 0.5});
        }
    }
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
    if (bypass_)
        return InteractionPoint{*projected, std::nullopt};
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
    if (nearest == nullptr && snapping_) {
        constexpr double axisTolerance = 1.0e-9;
        if (std::abs(projected->first) <= axisTolerance &&
            std::abs(projected->second) <= axisTolerance)
            return InteractionPoint{{0.0, 0.0},
                                    SketchPointReferenceIntent{0, SketchPointKind::origin}};
        if (std::abs(projected->second) <= axisTolerance)
            return InteractionPoint{{projected->first, 0.0},
                                    SketchPointReferenceIntent{0, SketchPointKind::xAxis}};
        if (std::abs(projected->first) <= axisTolerance)
            return InteractionPoint{{0.0, projected->second},
                                    SketchPointReferenceIntent{0, SketchPointKind::yAxis}};
    }
    if (nearest == nullptr) {
        constexpr kernel::PickingTolerance lineSnapRadius{8};
        const SceneSketchPoint* bestStart = nullptr;
        const SceneSketchPoint* bestEnd = nullptr;
        double bestDistance = lineSnapRadius.pixels;
        for (const SceneSketchPoint& start : snapPoints_) {
            if (start.elementId == 0 || start.kind != SketchPointKind::start)
                continue;
            // Point on object binds to lines only (S01); an arc's chord is not on the arc.
            if (std::ranges::any_of(snapPoints_, [&](const auto& candidate) {
                    return candidate.elementId == start.elementId &&
                           candidate.kind == SketchPointKind::center;
                }))
                continue;
            const auto end = std::ranges::find_if(snapPoints_, [&](const auto& candidate) {
                return candidate.elementId == start.elementId &&
                       candidate.kind == SketchPointKind::end;
            });
            if (end == snapPoints_.end())
                continue;
            int startX = 0;
            int startY = 0;
            int endX = 0;
            int endY = 0;
            view_->Convert(start.worldX, start.worldY, start.worldZ, startX, startY);
            view_->Convert(end->worldX, end->worldY, end->worldZ, endX, endY);
            const double dx = static_cast<double>(endX - startX);
            const double dy = static_cast<double>(endY - startY);
            const double squaredLength = dx * dx + dy * dy;
            if (squaredLength <= 1.0e-9)
                continue;
            const double parameter =
                std::clamp(((x - startX) * dx + (y - startY) * dy) / squaredLength, 0.0, 1.0);
            const double distance =
                std::hypot(startX + dx * parameter - x, startY + dy * parameter - y);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestStart = &start;
                bestEnd = &*end;
            }
        }
        if (bestStart != nullptr) {
            const double dx = bestEnd->localX - bestStart->localX;
            const double dy = bestEnd->localY - bestStart->localY;
            const double squaredLength = dx * dx + dy * dy;
            const double parameter = std::clamp(((projected->first - bestStart->localX) * dx +
                                                 (projected->second - bestStart->localY) * dy) /
                                                    squaredLength,
                                                0.0, 1.0);
            return InteractionPoint{
                {bestStart->localX + dx * parameter, bestStart->localY + dy * parameter},
                SketchPointReferenceIntent{bestStart->elementId, SketchPointKind::onObject}};
        }
    }
    if (nearest == nullptr)
        return InteractionPoint{*projected, std::nullopt};
    return InteractionPoint{
        {nearest->localX, nearest->localY},
        SketchPointReferenceIntent{nearest->elementId, nearest->kind, nearest->supportReferenceId,
                                   nearest->supportReferenceParameter, nearest->projectedEdge}};
}

SketchEditorInteraction::SnapFeedback SketchEditorInteraction::snapFeedback(int x, int y) const {
    SnapFeedback feedback;
    // Every drawing tool shows snap markers; selecting does not snap.
    if (!active_ || view_.IsNull() || tool_ == SketchElementTool::select)
        return feedback;
    constexpr int markerRadiusPixels = 60;
    if (!bypass_)
        for (const SceneSketchPoint& candidate : snapPoints_) {
            int pointX = 0;
            int pointY = 0;
            view_->Convert(candidate.worldX, candidate.worldY, candidate.worldZ, pointX, pointY);
            if ((pointX - x) * (pointX - x) + (pointY - y) * (pointY - y) <=
                markerRadiusPixels * markerRadiusPixels)
                feedback.candidates.emplace_back(pointX, pointY);
        }
    const auto resolved = interactionPoint(x, y);
    if (resolved.has_value() && resolved->reference.has_value()) {
        const gp_Vec xDirection(support_.xDirectionX, support_.xDirectionY, support_.xDirectionZ);
        const gp_Vec normal(support_.normalX, support_.normalY, support_.normalZ);
        const gp_Vec yDirection = normal.Crossed(xDirection);
        const gp_Pnt world = gp_Pnt(support_.originX, support_.originY, support_.originZ)
                                 .Translated(xDirection * resolved->position.first +
                                             yDirection * resolved->position.second);
        int pointX = 0;
        int pointY = 0;
        view_->Convert(world.X(), world.Y(), world.Z(), pointX, pointY);
        feedback.active = std::pair{pointX, pointY};
        feedback.activeKind = resolved->reference->point;
    }
    if (const auto intent = move(x, y))
        feedback.relation = intent->automaticConstraint;
    return feedback;
}

bool SketchEditorInteraction::valid(const SketchElementIntent& intent) const noexcept {
    constexpr double minimumSize = 1.0e-6;
    const double deltaX = intent.secondX - intent.firstX;
    const double deltaY = intent.secondY - intent.firstY;
    if (tool_ == SketchElementTool::rectangle || tool_ == SketchElementTool::centerRectangle) {
        return std::abs(deltaX) >= minimumSize && std::abs(deltaY) >= minimumSize;
    }
    if (needsThirdPoint(tool_) && intent.third.has_value()) {
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
    lastPointer_ = resolved->position;
    auto intent = finalIntent(resolved->position, resolved->reference);
    // D01: the fields show the element as it would be placed now (W11: its third point).
    if (intent && dynamic_.active() && intent->third)
        dynamic_.updateLiveThird(*firstPoint_, {intent->secondX, intent->secondY}, *intent->third);
    else if (intent && dynamic_.active())
        dynamic_.updateLive(*firstPoint_, {intent->secondX, intent->secondY});
    return intent;
}

// The element ended at `point`: automatic relations as the pointer suggests them, or (D01) the
// typed values, which fix the end and suggest no relation.
// D01/W11: the typed values fix the end (and the third point); no relation is suggested.
std::optional<SketchElementIntent>
SketchEditorInteraction::typedIntent(std::pair<double, double> point) const {
    const bool third = secondPoint_.has_value();
    const auto end = third ? *secondPoint_ : dynamic_.constrain(*firstPoint_, point);
    const auto thirdPoint =
        third ? std::optional{dynamic_.constrainThird(*firstPoint_, *secondPoint_, point)}
              : std::nullopt;
    SketchElementIntent intent{firstPoint_->first, firstPoint_->second, end.first,    end.second,
                               thirdPoint,         firstReference_,     std::nullopt, std::nullopt,
                               std::nullopt,       std::nullopt};
    intent.typed = dynamic_.typedDimensions();
    return valid(intent) ? std::optional{related(intent)} : std::nullopt;
}

std::optional<SketchElementIntent>
SketchEditorInteraction::finalIntent(std::pair<double, double> point,
                                     std::optional<SketchPointReferenceIntent> reference) const {
    if (dynamic_.hasTyped())
        return typedIntent(point);
    auto [adjustedPoint, automaticConstraint] = constrainedLineEnd(tool_, *firstPoint_, point);
    std::optional<std::uint64_t> automaticTargetId;
    if (!automaticConstraint.has_value())
        std::tie(adjustedPoint, automaticConstraint) = relatedLineEnd(*firstPoint_, point);
    if (!automaticConstraint.has_value() && reference.has_value() &&
        reference->point == SketchPointKind::onObject)
        automaticConstraint = SketchAutomaticConstraint::pointOnObject;
    if (tool_ == SketchElementTool::circle &&
        tangentToReferencedLine(*firstPoint_, point, reference))
        automaticConstraint = SketchAutomaticConstraint::tangent;
    else if (tool_ == SketchElementTool::circle && isRadialCenterReference(firstReference_)) {
        automaticConstraint = SketchAutomaticConstraint::concentric;
        automaticTargetId = firstReference_->elementId;
    } else if (tool_ == SketchElementTool::circle)
        std::tie(adjustedPoint, automaticConstraint, automaticTargetId) =
            relatedRadialPoint(*firstPoint_, point);
    std::pair<double, double> intentSecond =
        secondPoint_.has_value() ? *secondPoint_ : adjustedPoint;
    // S05: an arc ending on a line is not announced tangent; the solver adapter has no
    // tangency at an end point (end on the line plus tangency is redundant there).
    if (tool_ == SketchElementTool::arc && secondPoint_.has_value()) {
        if (isRadialCenterReference(firstReference_)) {
            automaticConstraint = SketchAutomaticConstraint::concentric;
            automaticTargetId = firstReference_->elementId;
        } else
            std::tie(intentSecond, automaticConstraint, automaticTargetId) =
                relatedRadialPoint(*firstPoint_, *secondPoint_);
    }
    point = adjustedPoint;
    const SketchElementIntent intent{firstPoint_->first,
                                     firstPoint_->second,
                                     intentSecond.first,
                                     intentSecond.second,
                                     secondPoint_.has_value() ? std::optional{point} : std::nullopt,
                                     firstReference_,
                                     secondPoint_.has_value() ? secondReference_ : reference,
                                     secondPoint_.has_value() ? reference : std::nullopt,
                                     automaticConstraint,
                                     automaticTargetId};
    return valid(intent) ? std::optional{related(intent)} : std::nullopt;
}

std::optional<SketchElementIntent> SketchEditorInteraction::click(int x, int y) {
    const auto resolved = interactionPoint(x, y);
    if (!resolved.has_value())
        return std::nullopt;
    auto [point, reference] = *resolved;
    // S15: a point is complete with its one click.
    if (tool_ == SketchElementTool::point)
        return SketchElementIntent{point.first,  point.second, point.first,  point.second,
                                   std::nullopt, reference,    std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt};
    if (!firstPoint_.has_value()) {
        firstPoint_ = point;
        firstReference_ = reference;
        dynamic_.begin(tool_);
        return std::nullopt;
    }
    if (needsThirdPoint(tool_) && !secondPoint_.has_value()) {
        const SketchElementIntent radiusIntent{
            firstPoint_->first, firstPoint_->second, point.first,  point.second, std::nullopt,
            std::nullopt,       std::nullopt,        std::nullopt, std::nullopt, std::nullopt};
        if (!valid(radiusIntent))
            return std::nullopt;
        placeSecond(point, reference);
        return std::nullopt;
    }
    auto intent = finalIntent(point, reference);
    if (!intent)
        return std::nullopt;
    finishElement();
    return intent;
}

// The second point of a tool of three points; typed values (W11) fix it and start the fields
// of the third point.
void SketchEditorInteraction::placeSecond(std::pair<double, double> point,
                                          std::optional<SketchPointReferenceIntent> reference) {
    const bool typed = dynamic_.hasTyped();
    secondPoint_ = typed ? dynamic_.constrain(*firstPoint_, point) : point;
    secondReference_ = typed ? std::nullopt : reference;
    dynamic_.beginThird(tool_);
}

void SketchEditorInteraction::finishElement() noexcept {
    firstPoint_.reset();
    secondPoint_.reset();
    firstReference_.reset();
    secondReference_.reset();
    dynamic_.reset();
}

// D01: Enter places the element with the typed values at the last pointer position; for a
// tool of three points (W11) the first Enter places the second point.
std::optional<SketchElementIntent> SketchEditorInteraction::commitTyped() {
    if (!firstPoint_.has_value() || !dynamic_.hasTyped() || dynamic_.hasInvalidText())
        return std::nullopt;
    const auto pointer = lastPointer_.value_or(*firstPoint_);
    if (needsThirdPoint(tool_) && !secondPoint_.has_value()) {
        placeSecond(pointer, std::nullopt);
        return std::nullopt;
    }
    auto intent = finalIntent(pointer, std::nullopt);
    if (intent)
        finishElement();
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

bool SketchEditorInteraction::cancelStep() noexcept {
    if (secondPoint_.has_value()) {
        secondPoint_.reset();
        secondReference_.reset();
        dynamic_.begin(tool_); // W11: back to the fields of the second point
        return true;
    }
    if (firstPoint_.has_value()) {
        firstPoint_.reset();
        firstReference_.reset();
        continued_.reset();
        dynamic_.reset();
        return true;
    }
    return false;
}

bool SketchEditorInteraction::isElementDragging() const noexcept { return dragPoint_.has_value(); }

} // namespace voelund::viewport
