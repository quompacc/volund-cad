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

void SketchEditorInteraction::continuePolyline(double x, double y,
                                               std::uint64_t elementId) noexcept {
    if (active_ && tool_ == SketchElementTool::polyline) {
        firstPoint_ = std::pair{x, y};
        firstReference_ = SketchPointReferenceIntent{elementId, SketchPointKind::end};
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
        snapPoints_.insert(snapPoints_.end(), support_.referencePoints.begin(),
                           support_.referencePoints.end());
        for (const PlanarReferenceSegment& segment : support_.referenceSegments) {
            if (support_.stableReferences)
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

std::pair<std::pair<double, double>, std::optional<SketchAutomaticConstraint>>
SketchEditorInteraction::relatedLineEnd(std::pair<double, double> first,
                                        std::pair<double, double> second) const {
    if (tool_ != SketchElementTool::line && tool_ != SketchElementTool::polyline)
        return {second, std::nullopt};
    const double rawX = second.first - first.first;
    const double rawY = second.second - first.second;
    const double length = std::hypot(rawX, rawY);
    double best = 0.08;
    std::pair<double, double> adjusted = second;
    std::optional<SketchAutomaticConstraint> relation;
    for (const SceneSketchPoint& start : snapPoints_) {
        if (start.elementId == 0 || start.kind != SketchPointKind::start)
            continue;
        const auto end = std::ranges::find_if(snapPoints_, [&](const auto& candidate) {
            return candidate.elementId == start.elementId && candidate.kind == SketchPointKind::end;
        });
        if (end == snapPoints_.end())
            continue;
        const double targetX = end->localX - start.localX;
        const double targetY = end->localY - start.localY;
        const double targetLength = std::hypot(targetX, targetY);
        if (length <= 1.0e-9 || targetLength <= 1.0e-9)
            continue;
        const double ux = targetX / targetLength;
        const double uy = targetY / targetLength;
        const double parallelError = std::abs(rawX * uy - rawY * ux) / length;
        if (parallelError < best) {
            best = parallelError;
            const double sign = rawX * ux + rawY * uy >= 0.0 ? 1.0 : -1.0;
            adjusted = {first.first + sign * ux * length, first.second + sign * uy * length};
            relation = SketchAutomaticConstraint::parallel;
        }
        const double perpendicularError = std::abs(rawX * ux + rawY * uy) / length;
        if (perpendicularError < best) {
            best = perpendicularError;
            const double nx = -uy;
            const double ny = ux;
            const double sign = rawX * nx + rawY * ny >= 0.0 ? 1.0 : -1.0;
            adjusted = {first.first + sign * nx * length, first.second + sign * ny * length};
            relation = SketchAutomaticConstraint::perpendicular;
        }
    }
    return {adjusted, relation};
}

bool SketchEditorInteraction::tangentToReferencedLine(
    std::pair<double, double> center, std::pair<double, double> radialPoint,
    const std::optional<SketchPointReferenceIntent>& reference) const {
    if (!reference.has_value() || reference->point != SketchPointKind::onObject)
        return false;
    const auto start = std::ranges::find_if(snapPoints_, [&](const auto& candidate) {
        return candidate.elementId == reference->elementId &&
               candidate.kind == SketchPointKind::start;
    });
    const auto end = std::ranges::find_if(snapPoints_, [&](const auto& candidate) {
        return candidate.elementId == reference->elementId &&
               candidate.kind == SketchPointKind::end;
    });
    if (start == snapPoints_.end() || end == snapPoints_.end())
        return false;
    const double radiusX = radialPoint.first - center.first;
    const double radiusY = radialPoint.second - center.second;
    const double lineX = end->localX - start->localX;
    const double lineY = end->localY - start->localY;
    const double scale = std::hypot(radiusX, radiusY) * std::hypot(lineX, lineY);
    return scale > 1.0e-9 && std::abs(radiusX * lineX + radiusY * lineY) / scale <= 0.08;
}

std::tuple<std::pair<double, double>, std::optional<SketchAutomaticConstraint>,
           std::optional<std::uint64_t>>
SketchEditorInteraction::relatedRadialPoint(std::pair<double, double> center,
                                            std::pair<double, double> radialPoint) const {
    const double dx = radialPoint.first - center.first;
    const double dy = radialPoint.second - center.second;
    const double radius = std::hypot(dx, dy);
    if (radius <= 1.0e-9)
        return {radialPoint, std::nullopt, std::nullopt};
    double bestError = 0.08;
    double bestRadius = radius;
    std::optional<SketchAutomaticConstraint> relation;
    std::optional<std::uint64_t> targetId;
    for (const SceneSketchPoint& target : snapPoints_) {
        if (target.kind != SketchPointKind::center || target.elementId == 0 ||
            target.radialRadius <= 1.0e-9)
            continue;
        const double centerDistance =
            std::hypot(center.first - target.localX, center.second - target.localY);
        const auto consider = [&](double candidateRadius, SketchAutomaticConstraint candidate) {
            if (candidateRadius <= 1.0e-9)
                return;
            const double error =
                std::abs(radius - candidateRadius) / std::max(radius, candidateRadius);
            if (error < bestError ||
                (candidate == SketchAutomaticConstraint::tangent && error <= bestError + 1.0e-12)) {
                bestError = error;
                bestRadius = candidateRadius;
                relation = candidate;
                targetId = target.elementId;
            }
        };
        consider(target.radialRadius, SketchAutomaticConstraint::equal);
        consider(centerDistance - target.radialRadius, SketchAutomaticConstraint::tangent);
        consider(std::abs(target.radialRadius - centerDistance),
                 SketchAutomaticConstraint::tangent);
        consider(centerDistance + target.radialRadius, SketchAutomaticConstraint::tangent);
    }
    if (!relation.has_value())
        return {radialPoint, std::nullopt, std::nullopt};
    return {{center.first + dx * bestRadius / radius, center.second + dy * bestRadius / radius},
            relation,
            targetId};
}

bool SketchEditorInteraction::isRadialCenterReference(
    const std::optional<SketchPointReferenceIntent>& reference) const {
    if (!reference.has_value() || reference->elementId == 0 ||
        reference->point != SketchPointKind::center)
        return false;
    return std::ranges::any_of(snapPoints_, [&](const SceneSketchPoint& point) {
        return point.elementId == reference->elementId && point.kind == SketchPointKind::center &&
               point.radialRadius > 1.0e-9;
    });
}

bool SketchEditorInteraction::valid(const SketchElementIntent& intent) const noexcept {
    constexpr double minimumSize = 1.0e-6;
    const double deltaX = intent.secondX - intent.firstX;
    const double deltaY = intent.secondY - intent.firstY;
    if (tool_ == SketchElementTool::rectangle) {
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
    auto [point, reference] = *resolved;
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
    if (tool_ == SketchElementTool::arc && secondPoint_.has_value() &&
        tangentToReferencedLine(*firstPoint_, point, reference))
        automaticConstraint = SketchAutomaticConstraint::tangent;
    std::pair<double, double> intentSecond =
        secondPoint_.has_value() ? *secondPoint_ : adjustedPoint;
    if (tool_ == SketchElementTool::arc && secondPoint_.has_value() &&
        automaticConstraint != SketchAutomaticConstraint::tangent) {
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
    return valid(intent) ? std::optional{intent} : std::nullopt;
}

std::optional<SketchElementIntent> SketchEditorInteraction::click(int x, int y) {
    const auto resolved = interactionPoint(x, y);
    if (!resolved.has_value())
        return std::nullopt;
    auto [point, reference] = *resolved;
    if (!firstPoint_.has_value()) {
        firstPoint_ = point;
        firstReference_ = reference;
        return std::nullopt;
    }
    if (needsThirdPoint(tool_) && !secondPoint_.has_value()) {
        const SketchElementIntent radiusIntent{
            firstPoint_->first, firstPoint_->second, point.first,  point.second, std::nullopt,
            std::nullopt,       std::nullopt,        std::nullopt, std::nullopt, std::nullopt};
        if (!valid(radiusIntent))
            return std::nullopt;
        secondPoint_ = point;
        secondReference_ = reference;
        return std::nullopt;
    }
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
    if (tool_ == SketchElementTool::arc && secondPoint_.has_value() &&
        tangentToReferencedLine(*firstPoint_, point, reference))
        automaticConstraint = SketchAutomaticConstraint::tangent;
    std::pair<double, double> intentSecond =
        secondPoint_.has_value() ? *secondPoint_ : adjustedPoint;
    if (tool_ == SketchElementTool::arc && secondPoint_.has_value() &&
        automaticConstraint != SketchAutomaticConstraint::tangent) {
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

bool SketchEditorInteraction::cancelStep() noexcept {
    if (secondPoint_.has_value()) {
        secondPoint_.reset();
        secondReference_.reset();
        return true;
    }
    if (firstPoint_.has_value()) {
        firstPoint_.reset();
        firstReference_.reset();
        return true;
    }
    return false;
}

bool SketchEditorInteraction::isElementDragging() const noexcept { return dragPoint_.has_value(); }

} // namespace voelund::viewport
