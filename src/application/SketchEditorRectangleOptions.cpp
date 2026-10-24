// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <variant>

// W13 (user report 2026-10-24, as in FreeCAD): the rectangle tool's options "Abgerundete
// Ecken" and "Rahmen". A rectangle just drawn gets
// - its width and height as dimensions (unless typed already),
// - with a frame, an inner rectangle whose sides stand the frame width off the outer ones,
// - with rounded corners, every corner rounded (the sketch fillet, S11); the corners share one
//   radius through equal relations, so one radius dimension sets them all; inside a frame the
//   inner corners are rounded too, by the radius less the frame width.
// Everything but the position is then fixed; snapped to the origin, nothing is left free.
namespace voelund::application {
namespace {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using Position = document::SketchPointPosition;
using Sides = SketchEditorSession::RectangleSides;

// The sides in contour order: side i ends where side i + 1 starts.
Sides sidesOf(const SketchCreationResult& created) {
    constexpr std::array roles{
        SketchCreationRole::rectangleFromFirstCorner, SketchCreationRole::rectangleToSecondCorner,
        SketchCreationRole::rectangleFromSecondCorner, SketchCreationRole::rectangleToFirstCorner};
    Sides sides{SketchElementId{0}, SketchElementId{0}, SketchElementId{0}, SketchElementId{0}};
    for (std::size_t index = 0; index < roles.size(); ++index) {
        const auto found =
            std::ranges::find(created.elements, roles[index], &SketchCreatedElement::role);
        if (found == created.elements.end())
            throw std::invalid_argument("Rechteck: Eine Seite fehlt");
        sides[index] = found->id;
    }
    return sides;
}

// A value as a drafter picks it: whole half millimetres, at least half a millimetre.
double roundHalf(double value) { return std::max(0.5, std::round(value * 2) / 2); }

struct Sizes final {
    double radius;
    double frame;
};

// The corner radius and frame width from the steps, else a share of the shorter side; both
// must fit the rectangle.
Sizes sizesFor(const SketchEditorSession::RectangleOptions& options, double shorter) {
    const double frame = options.frameWidth.value_or(roundHalf(0.1 * shorter));
    const double least = options.frame && frame > 0 ? frame + 0.5 : 0.0;
    const double radius = options.radius.value_or(std::max(roundHalf(0.15 * shorter), least));
    if (2 * frame >= shorter || 2 * radius >= shorter || radius <= 0.0 || frame == 0.0)
        throw std::invalid_argument("Rechteck: Für Rahmen oder Rundung zu klein");
    return {radius, frame};
}

} // namespace

document::SketchLine SketchEditorSession::lineOf(SketchElementId id) const {
    const auto match = std::ranges::find(elements_.values, id, &document::SketchElement::id);
    if (match == elements_.values.end() ||
        !std::holds_alternative<document::SketchLine>(match->geometry))
        throw std::invalid_argument("Rechteck: Eine Seite ist keine Linie");
    return std::get<document::SketchLine>(match->geometry);
}

void SketchEditorSession::addRelation(SketchConstraintKind kind, SketchGeometryReference first,
                                      std::optional<SketchGeometryReference> second, double value) {
    constraints_.values.push_back(
        {document::SketchConstraintId{constraints_.nextId++}, kind, first, second, value});
}

// Width and height of the outer rectangle, unless a length is on that side or the opposite
// one already (typed while drawing).
void SketchEditorSession::dimensionSides(const Sides& sides) {
    for (std::size_t index : {0U, 1U}) {
        const auto side = sides[index];
        const auto opposite = sides[index + 2];
        const bool known = std::ranges::any_of(constraints_.values, [&](const auto& constraint) {
            return constraint.kind == SketchConstraintKind::distance &&
                   (constraint.first.element == side || constraint.first.element == opposite);
        });
        const auto line = lineOf(side);
        if (!known)
            addRelation(SketchConstraintKind::distance, {side, Position::start}, std::nullopt,
                        std::hypot(line.end.x - line.start.x, line.end.y - line.start.y));
    }
}

// The frame rectangle, each side `width` off the drawn one (inwards above zero, outwards
// below); its sides in contour order.
Sides SketchEditorSession::addFrame(const Sides& outer, double width) {
    const auto first = lineOf(outer[0]).start;  // the first corner
    const auto second = lineOf(outer[2]).start; // the opposite one
    const double sx = second.x > first.x ? 1.0 : -1.0;
    const double sy = second.y > first.y ? 1.0 : -1.0;
    const auto inner = confirmRectangle(document::SketchRectangle{
        first.x + sx * width, first.y + sy * width, second.x - sx * width, second.y - sy * width});
    const Sides sides = sidesOf(inner);
    for (std::size_t index = 0; index < sides.size(); ++index)
        addRelation(SketchConstraintKind::pointLineDistance, {sides[index], Position::start},
                    SketchGeometryReference{outer[index], Position::start}, std::abs(width));
    return sides;
}

// Rounds all four corners with `radius`; the first arc keeps the radius dimension, the others
// are equal to it.
void SketchEditorSession::roundCorners(const Sides& sides, double radius) {
    std::vector<SketchElementId> arcs;
    for (std::size_t index = 0; index < sides.size(); ++index) {
        const auto next = sides[(index + 1) % sides.size()];
        const auto corner = lineOf(sides[index]).end;
        clearSelection();
        if (!selectNearestReference(sides[index], corner, false) ||
            !selectNearestReference(next, corner, true))
            throw std::invalid_argument("Rechteck: Ecke nicht gefunden");
        const auto rounded = filletCorner(radius);
        if (!rounded.created)
            throw std::invalid_argument("Rechteck: Ecke lässt sich nicht verrunden");
        arcs.push_back(*rounded.created);
    }
    for (auto& constraint : constraints_.values)
        if (constraint.kind == SketchConstraintKind::radius &&
            constraint.first.element != arcs[0] &&
            std::ranges::find(arcs, constraint.first.element) != arcs.end()) {
            constraint.kind = SketchConstraintKind::equal;
            constraint.second = SketchGeometryReference{arcs[0], Position::start};
            constraint.value = 0.0;
        }
}

void SketchEditorSession::finishRectangle(const SketchCreationResult& created,
                                          RectangleOptions options) {
    if (!options.rounded && !options.frame)
        return;
    Rollback rollback(*this);
    const Sides outer = sidesOf(created);
    const auto width = lineOf(outer[0]), height = lineOf(outer[1]);
    const double shorter =
        std::min(std::hypot(width.end.x - width.start.x, width.end.y - width.start.y),
                 std::hypot(height.end.x - height.start.x, height.end.y - height.start.y));
    const auto [radius, frame] = sizesFor(options, shorter);
    dimensionSides(outer);
    std::optional<Sides> inner;
    if (options.frame)
        inner = addFrame(outer, frame);
    if (options.rounded) {
        roundCorners(outer, radius);
        // The frame keeps one thickness: its corners are concentric with the drawn ones.
        if (inner && radius - frame > 1e-6)
            roundCorners(*inner, radius - frame);
    }
    clearSelection();
    if (!keepIfValid(rollback))
        throw std::invalid_argument("Rechteck: Rundung oder Rahmen lässt sich nicht lösen");
}

} // namespace voelund::application
