// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include <QObject>

#include <algorithm>
#include <variant>

// W02 (as in Fusion and SolidCraft): the dimension tool (D). A click on a line gives its
// length, on a circle its diameter, on an arc its radius; the measured value opens selected in
// the field at the pointer, so typing and Enter set it. A second click before anything is
// typed turns the dimension into one between both picks: angle of two lines, distance of a
// point to a line or of two points. Escape keeps the measured dimension; the tool stays on.
namespace voelund::desktop {
namespace {

using document::SketchConstraintKind;

// The dimension a single pick suggests: a circle's diameter, an arc's radius, a line's length.
std::optional<SketchConstraintKind> singleDimension(const application::SketchEditorSession& editor,
                                                    document::SketchElementId element) {
    const auto match = std::ranges::find(editor.elements(), element, &document::SketchElement::id);
    if (match == editor.elements().end())
        return std::nullopt;
    if (std::holds_alternative<document::SketchCircle>(match->geometry))
        return SketchConstraintKind::diameter;
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

bool SketchEditorToolController::pickForDimension(const viewport::SketchSelectionIntent& intent) {
    const bool pairing = dimensionAnchor_.has_value();
    if (pairing) {
        // The single dimension of the first pick gives way to one between both picks.
        static_cast<void>(
            editor_->removeConstraint(document::SketchConstraintId{*dimensionAnchor_}));
        dimensionAnchor_.reset();
    }
    const document::SketchElementId element{intent.elementId};
    if (!editor_->selectNearestReference(element, {intent.localX, intent.localY}, pairing))
        return false;
    const bool pair = editor_->selectedReferences().size() >= 2;
    const auto kind = pair ? pairDimension(*editor_) : singleDimension(*editor_, element);
    if (!kind || !editor_->addSelectedRelation(*kind)) {
        editor_->clearSelection();
        refreshPresentation();
        statusChanged_(QObject::tr("Für diese Auswahl gibt es kein Maß"));
        return true;
    }
    const auto created = newestConstraint(*editor_);
    if (pair)
        editor_->clearSelection();
    else if (created)
        dimensionAnchor_ = created->value();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (!created)
        return true;
    statusChanged_(pair ? QObject::tr("Maß zwischen beiden: Wert tippen, Enter")
                        : QObject::tr("Wert tippen und Enter, oder ein zweites Element anklicken"));
    const auto angle = editor_->constraintAngle(*created);
    const double value = angle ? kernel::toDegrees(*angle).value()
                               : editor_->constraintValue(*created).value_or(0.0);
    openDimensionEdit(created->value(), QString::number(value, 'g', 12));
    return true;
}

} // namespace voelund::desktop
