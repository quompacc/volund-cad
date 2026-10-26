// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace voelund::application {
namespace {

using document::SketchPoint;

struct TangentArc final {
    document::SketchArc arc;
    bool startsAtLine; // the arc's start (counterclockwise) is the line's end
};

// The arc through `end` that leaves `start` along `along` (unit): its centre lies on the normal
// at `start`, as far from `end` as from `start`.
std::optional<TangentArc> tangentArc(SketchPoint start, SketchPoint along, SketchPoint end) {
    const SketchPoint normal{-along.y, along.x};
    const double vx = end.x - start.x, vy = end.y - start.y;
    const double across = 2 * (vx * normal.x + vy * normal.y);
    if (std::abs(across) < 1e-9)
        return std::nullopt; // on the line's extension: no arc
    const double radius = (vx * vx + vy * vy) / across;
    const SketchPoint centre{start.x + normal.x * radius, start.y + normal.y * radius};
    const double atStart = std::atan2(start.y - centre.y, start.x - centre.x);
    const double atEnd = std::atan2(end.y - centre.y, end.x - centre.x);
    // Counterclockwise the tangent at the start is the radius turned left.
    const bool counterclockwise = radius > 0;
    return TangentArc{{centre, std::abs(radius), counterclockwise ? atStart : atEnd,
                       counterclockwise ? atEnd : atStart},
                      counterclockwise};
}

} // namespace

SketchCreationResult
SketchEditorSession::createTangentArc(document::SketchGeometryReference lineEnd,
                                      document::SketchPoint end) {
    const auto line =
        std::ranges::find(elements_.values, lineEnd.element, &document::SketchElement::id);
    const auto* geometry = line == elements_.values.end()
                               ? nullptr
                               : std::get_if<document::SketchLine>(&line->geometry);
    if (!active_ || geometry == nullptr || lineEnd.point == document::SketchPointPosition::center)
        throw std::invalid_argument("Tangentialbogen: zuerst auf das Ende einer Linie klicken");
    const bool atEnd = lineEnd.point == document::SketchPointPosition::end;
    const SketchPoint start = atEnd ? geometry->end : geometry->start;
    const SketchPoint other = atEnd ? geometry->start : geometry->end;
    const double length = std::hypot(start.x - other.x, start.y - other.y);
    const auto arc =
        length < 1e-9
            ? std::nullopt
            : tangentArc(start, {(start.x - other.x) / length, (start.y - other.y) / length}, end);
    if (!arc)
        throw std::invalid_argument("Tangentialbogen: der Endpunkt liegt auf der Verlängerung");
    Rollback rollback(*this);
    const document::SketchElementId id{elements_.nextId++};
    elements_.values.push_back({id, arc->arc});
    using Kind = document::SketchConstraintKind;
    const document::SketchGeometryReference joint{id, arc->startsAtLine
                                                          ? document::SketchPointPosition::start
                                                          : document::SketchPointPosition::end};
    const document::SketchConstraintId coincident{constraints_.nextId++};
    constraints_.values.push_back({coincident, Kind::coincident, lineEnd, joint, 0.0});
    const document::SketchConstraintId tangent{constraints_.nextId++};
    constraints_.values.push_back(
        {tangent, Kind::tangent, {lineEnd.element}, document::SketchGeometryReference{id}, 0.0});
    clearPreview();
    selectedElement_ = id;
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    rollback.keep();
    commitHistory();
    return SketchCreationResult{
        {{id, SketchCreationRole::element}},
        {},
        {{coincident, SketchConstraintOrigin::shape}, {tangent, SketchConstraintOrigin::shape}}};
}

} // namespace voelund::application
