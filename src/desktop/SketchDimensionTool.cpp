// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <variant>

// W02 (as in Fusion and SolidCraft): the dimension tool (D). A click on a line gives its
// length, on a circle or an arc its radius; the measured value opens selected in
// the field at the pointer, so typing and Enter set it. A second click before anything is
// typed turns the dimension into one between both picks: angle of two lines, distance of a
// point to a line or of two points. Escape keeps the measured dimension; the tool stays on.
namespace voelund::desktop {
namespace {

using document::SketchConstraintKind;

// The dimension a single pick suggests: a circle's or an arc's radius, a line's length.
std::optional<SketchConstraintKind> singleDimension(const application::SketchEditorSession& editor,
                                                    document::SketchElementId element) {
    const auto match = std::ranges::find(editor.elements(), element, &document::SketchElement::id);
    if (match == editor.elements().end())
        return std::nullopt;
    if (std::holds_alternative<document::SketchCircle>(match->geometry))
        return SketchConstraintKind::radius;
    if (std::holds_alternative<document::SketchArc>(match->geometry))
        return SketchConstraintKind::radius;
    if (std::holds_alternative<document::SketchLine>(match->geometry))
        return SketchConstraintKind::distance;
    return std::nullopt;
}

// The first dimension two picks allow, in the order a CAD user expects it.
std::optional<SketchConstraintKind> pairDimension(const application::SketchEditorSession& editor) {
    for (const auto kind : {SketchConstraintKind::angle, SketchConstraintKind::pointLineDistance,
                            SketchConstraintKind::pointDistance})
        if (editor.canAddSelectedRelation(kind))
            return kind;
    return std::nullopt;
}

std::optional<document::SketchConstraintId>
newestConstraint(const application::SketchEditorSession& editor) {
    const auto values = editor.constraints();
    const auto newest = std::ranges::max_element(
        values, {}, [](const auto& constraint) { return constraint.id.value(); });
    return newest == values.end() ? std::nullopt : std::optional{newest->id};
}

} // namespace

// One pick: a line's own length (as the length field of the panel makes it, not a distance of
// its end point), a circle's or an arc's radius. An existing dimension is reused.
std::optional<document::SketchConstraintId>
SketchEditorToolController::singleDimensionFor(document::SketchElementId element) {
    const auto kind = singleDimension(*editor_, element);
    if (!kind)
        return std::nullopt;
    if (*kind == SketchConstraintKind::distance) {
        if (!editor_->selectElement(element))
            return std::nullopt;
        const auto match =
            std::ranges::find(editor_->elements(), element, &document::SketchElement::id);
        const auto& line = std::get<document::SketchLine>(match->geometry);
        static_cast<void>(editor_->setSelectedLineLength(
            std::hypot(line.end.x - line.start.x, line.end.y - line.start.y)));
        return editor_->selectedDimension(SketchConstraintKind::distance);
    }
    // Circles and arcs as the radius field of the panel makes it.
    if (!editor_->selectElement(element))
        return std::nullopt;
    const auto match =
        std::ranges::find(editor_->elements(), element, &document::SketchElement::id);
    const auto* circle = std::get_if<document::SketchCircle>(&match->geometry);
    const auto* arc = std::get_if<document::SketchArc>(&match->geometry);
    static_cast<void>(editor_->setSelectedCircleRadius(circle ? circle->radius : arc->radius));
    return editor_->selectedDimension(SketchConstraintKind::radius);
}

bool SketchEditorToolController::pickForDimension(const viewport::SketchSelectionIntent& intent) {
    const document::SketchElementId element{intent.elementId};
    const document::SketchPoint at{intent.localX, intent.localY};
    std::optional<document::SketchConstraintId> created;
    bool pair = false;
    if (dimensionAnchor_ && dimensionFirst_ && dimensionFirst_->first != element) {
        // The single dimension of the first pick gives way to one between both picks.
        static_cast<void>(
            editor_->removeConstraint(document::SketchConstraintId{*dimensionAnchor_}));
        dimensionAnchor_.reset();
        pair = editor_->selectNearestReference(dimensionFirst_->first, dimensionFirst_->second,
                                               false) &&
               editor_->selectNearestReference(element, at, true);
        const auto kind = pair ? pairDimension(*editor_) : std::nullopt;
        if (kind && editor_->addSelectedRelation(*kind))
            created = newestConstraint(*editor_);
        dimensionFirst_.reset();
    } else {
        created = singleDimensionFor(element);
        dimensionFirst_ = std::pair{element, at};
        dimensionAnchor_ = created ? std::optional{created->value()} : std::nullopt;
    }
    editor_->clearSelection();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (!created) {
        dimensionAnchor_.reset();
        dimensionFirst_.reset();
        statusChanged_(QObject::tr("Für diese Auswahl gibt es kein Maß"));
        return true;
    }
    statusChanged_(pair ? QObject::tr("Maß zwischen beiden: Wert tippen, Enter")
                        : QObject::tr("Wert tippen und Enter, oder ein zweites Element anklicken"));
    const auto angle = editor_->constraintAngle(*created);
    const double value = angle ? kernel::toDegrees(*angle).value()
                               : editor_->constraintValue(*created).value_or(0.0);
    openDimensionEdit(created->value(), QString::number(std::abs(value), 'g', 12));
    return true;
}

} // namespace voelund::desktop
