// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchLooseEnds.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace voelund::features {
namespace {

using document::SketchGeometryReference;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double touching = 1e-6;

struct End final {
    SketchGeometryReference reference;
    SketchPoint at;
};

// The two ends of an open curve; none for closed ones.
std::vector<End> endsOf(const document::SketchElement& element) {
    if (const auto* line = std::get_if<document::SketchLine>(&element.geometry))
        return {{{element.id, Position::start}, line->start},
                {{element.id, Position::end}, line->end}};
    if (const auto* arc = std::get_if<document::SketchArc>(&element.geometry)) {
        const auto on = [&](double angle) {
            return SketchPoint{arc->center.x + arc->radius * std::cos(angle),
                               arc->center.y + arc->radius * std::sin(angle)};
        };
        return {{{element.id, Position::start}, on(arc->startAngle)},
                {{element.id, Position::end}, on(arc->endAngle)}};
    }
    if (const auto* arc = std::get_if<document::SketchEllipticArc>(&element.geometry)) // N14c
        return {{{element.id, Position::start},
                 document::ellipsePoint(arc->ellipse(), arc->startParameter)},
                {{element.id, Position::end},
                 document::ellipsePoint(arc->ellipse(), arc->endParameter)}};
    return {};
}

// An end a relation already places: coincident with a point, on a curve, at a midpoint.
bool tied(const SketchGeometryReference& end,
          std::span<const document::SketchConstraint> constraints) {
    return std::ranges::any_of(constraints, [&](const document::SketchConstraint& constraint) {
        using Kind = document::SketchConstraintKind;
        if (constraint.kind != Kind::coincident && constraint.kind != Kind::pointOnObject &&
            constraint.kind != Kind::midpoint)
            return false;
        return constraint.first == end || (constraint.second && *constraint.second == end);
    });
}

bool same(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(a.x - b.x, a.y - b.y) < touching;
}

// The ends of the non-construction curves that no relation ties.
std::vector<End> untiedEnds(std::span<const document::SketchElement> elements,
                            std::span<const document::SketchConstraint> constraints) {
    std::vector<End> loose;
    for (const auto& element : elements) {
        if (element.construction)
            continue;
        for (const End& end : endsOf(element))
            if (!tied(end.reference, constraints))
                loose.push_back(end);
    }
    return loose;
}

// A later loose end of another curve at the same place, not yet paired.
std::optional<std::size_t> partnerOf(const std::vector<End>& loose, const std::vector<bool>& paired,
                                     std::size_t index) {
    for (std::size_t other = index + 1; other < loose.size(); ++other)
        if (!paired[other] && loose[other].reference.element != loose[index].reference.element &&
            same(loose[index].at, loose[other].at))
            return other;
    return std::nullopt;
}

} // namespace

SketchLooseEnds sketchLooseEnds(std::span<const document::SketchElement> elements,
                                std::span<const document::SketchConstraint> constraints) {
    const std::vector<End> loose = untiedEnds(elements, constraints);
    SketchLooseEnds result;
    std::vector<bool> paired(loose.size(), false);
    for (std::size_t index = 0; index < loose.size(); ++index) {
        if (paired[index])
            continue;
        if (const auto partner = partnerOf(loose, paired, index)) {
            paired[index] = paired[*partner] = true;
            result.untied.emplace_back(loose[index].reference, loose[*partner].reference);
        } else {
            result.open.push_back(loose[index].reference);
        }
    }
    return result;
}

int sketchFreedomWithoutRelations(std::span<const document::SketchElement> elements) {
    int total = 0;
    for (const auto& element : elements) {
        const auto& geometry = element.geometry;
        if (std::holds_alternative<document::SketchCircle>(geometry))
            total += 3;
        else if (std::holds_alternative<document::SketchArc>(geometry) ||
                 std::holds_alternative<document::SketchEllipse>(geometry))
            total += 5;
        else if (std::holds_alternative<document::SketchPointGeometry>(geometry))
            total += 2;
        else if (std::holds_alternative<document::SketchEllipticArc>(geometry))
            total += 7; // N14c: the ellipse's five and the two end parameters
        else if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry))
            total += 2 * static_cast<int>(spline->poles.size());
        else
            total += 4; // a line, a rectangle's two corners
    }
    return total;
}

} // namespace voelund::features
