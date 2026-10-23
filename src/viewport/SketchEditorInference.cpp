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

// Automatic relations a drawn line or radius suggests (moved from SketchEditorInteraction.cpp
// for its source size).
namespace voelund::viewport {

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

} // namespace voelund::viewport
