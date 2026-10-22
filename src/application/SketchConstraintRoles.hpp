// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

// S10/S11: which references of a relation name points, and which relations fix a line's
// length; shared by the edits that move end points (trim, extend, split, fillet).
namespace voelund::application::detail {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;

// Whether the first or second reference of the relation names a point (not a whole element).
inline bool firstIsPoint(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::coincident:
    case SketchConstraintKind::pointDistance:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
    case SketchConstraintKind::horizontalPoints:
    case SketchConstraintKind::verticalPoints:
    case SketchConstraintKind::coordinateX:
    case SketchConstraintKind::coordinateY:
    case SketchConstraintKind::symmetric:
    case SketchConstraintKind::pointOnObject:
    case SketchConstraintKind::midpoint:
    case SketchConstraintKind::pointLineDistance:
        return true;
    default:
        return false;
    }
}
inline bool secondIsPoint(SketchConstraintKind kind) {
    return firstIsPoint(kind) && kind != SketchConstraintKind::coordinateX &&
           kind != SketchConstraintKind::coordinateY &&
           kind != SketchConstraintKind::pointOnObject && kind != SketchConstraintKind::midpoint &&
           kind != SketchConstraintKind::pointLineDistance;
}

template <typename Visit> void visitPoints(SketchConstraint& constraint, Visit visit) {
    if (firstIsPoint(constraint.kind))
        visit(constraint.first);
    if (secondIsPoint(constraint.kind) && constraint.second.has_value())
        visit(*constraint.second);
}

inline bool names(const SketchConstraint& constraint, SketchElementId element) {
    return constraint.first.element == element ||
           (constraint.second.has_value() && constraint.second->element == element);
}

// Relations that fix the length or extent of a line.
inline bool bindsLength(const SketchConstraint& constraint, SketchElementId line) {
    switch (constraint.kind) {
    case SketchConstraintKind::distance:
    case SketchConstraintKind::fixed:
        return constraint.first.element == line;
    case SketchConstraintKind::equal:
    case SketchConstraintKind::perpendicularBisector:
        return names(constraint, line);
    case SketchConstraintKind::midpoint:
        return constraint.second->element == line;
    default:
        return false;
    }
}

} // namespace voelund::application::detail
