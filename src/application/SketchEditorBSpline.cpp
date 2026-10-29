// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchSplineFit.hpp"
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

// S21 (user decision 2026-10-14): an open cubic B-spline over control points, as one step. An
// end placed on the end of a line, arc or other B-spline is joined to it (coincident); a moved
// control point changes the curve, and the solve keeps the joints.
namespace voelund::application {
namespace {

using document::SketchGeometryReference;
using document::SketchPoint;
using Position = document::SketchPointPosition;

std::vector<std::pair<SketchGeometryReference, SketchPoint>>
ends(const document::SketchElement& element) {
    const auto& geometry = element.geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return {{{element.id, Position::start}, line->start},
                {{element.id, Position::end}, line->end}};
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
        return {{{element.id, Position::start},
                 {arc->center.x + arc->radius * std::cos(arc->startAngle),
                  arc->center.y + arc->radius * std::sin(arc->startAngle)}},
                {{element.id, Position::end},
                 {arc->center.x + arc->radius * std::cos(arc->endAngle),
                  arc->center.y + arc->radius * std::sin(arc->endAngle)}}};
    if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry))
        return {{{element.id, Position::start}, spline->poles.front()},
                {{element.id, Position::end}, spline->poles.back()}};
    return {};
}

} // namespace

std::optional<document::SketchElementId>
SketchEditorSession::addBSpline(std::span<const SketchPoint> poles, bool construction) {
    if (!active_ || poles.size() < document::minimumBSplinePoles ||
        poles.size() > document::maximumBSplinePoles || !canAllocateIds(1, 2))
        return std::nullopt;
    Rollback rollback(*this);
    clearPreview();
    const document::SketchElementId id{elements_.nextId++};
    const document::SketchBSpline spline{{poles.begin(), poles.end()}, {}};
    // Joints with ends that the new ends were snapped onto.
    std::vector<std::pair<SketchGeometryReference, SketchGeometryReference>> joints;
    for (const auto& element : elements_.values)
        for (const auto& [reference, point] : ends(element))
            for (const auto& [own, end] : {std::pair{Position::start, spline.poles.front()},
                                           std::pair{Position::end, spline.poles.back()}})
                if (std::hypot(point.x - end.x, point.y - end.y) < 1e-9)
                    joints.push_back({{id, own}, reference});
    elements_.values.push_back({id, spline, construction});
    for (const auto& [own, other] : joints)
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                       document::SketchConstraintKind::coincident, own, other,
                                       0.0});
    selectedElement_ = id;
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (!keepIfValid(rollback))
        return std::nullopt;
    commitHistory();
    return id;
}

std::optional<document::SketchElementId>
SketchEditorSession::addSplineThroughPoints(std::span<const SketchPoint> points,
                                            bool construction) {
    const auto poles = detail::splineThroughPoints(points);
    if (poles.empty())
        return std::nullopt;
    return addBSpline(poles, construction);
}

bool SketchEditorSession::moveBSplinePole(document::SketchElementId element, std::size_t pole,
                                          SketchPoint position) {
    const auto match = std::ranges::find(elements_.values, element, &document::SketchElement::id);
    if (!active_ || match == elements_.values.end() || isProjected(element) ||
        isPatternInstance(element) || !std::isfinite(position.x) || !std::isfinite(position.y))
        return false;
    auto* spline = std::get_if<document::SketchBSpline>(&match->geometry);
    if (spline == nullptr || pole >= spline->poles.size())
        return false;
    Rollback rollback(*this);
    spline->poles[pole] = position;
    if (!keepIfValid(rollback))
        return false;
    commitHistory();
    return true;
}

} // namespace voelund::application
