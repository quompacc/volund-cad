// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include <QCursor>
#include <QMenu>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <variant>

// W02 (as in Fusion and SolidCraft): the dimension tool (D). A click on a line gives its
// length, on a circle or an arc its radius; the measured value opens selected in
// the field at the pointer, so typing and Enter set it. A second click before anything is
// typed turns the dimension into one between both picks.
// VP02 (user review 2026-10-30): what the pointer is near decides what is picked. A click near
// a corner or center takes that point and waits for the second pick; two points give their
// distance, a point and a line the distance between them, two lines their angle. Two points
// that lie apart in both directions ask which distance is meant. Escape keeps the measured
// dimension; the tool stays on.
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

// The first dimension two picks allow, in the order a CAD user expects it. Two points mean
// their distance, a point and a curve the distance between them; only two curves their angle.
std::optional<SketchConstraintKind> pairDimension(const application::SketchEditorSession& editor,
                                                  int pointsPicked) {
    if (pointsPicked == 2)
        return editor.canAddSelectedRelation(SketchConstraintKind::pointDistance)
                   ? std::optional{SketchConstraintKind::pointDistance}
                   : std::nullopt;
    if (pointsPicked == 0 && editor.canAddSelectedRelation(SketchConstraintKind::angle))
        return SketchConstraintKind::angle;
    for (const auto kind :
         {SketchConstraintKind::pointLineDistance, SketchConstraintKind::pointDistance})
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

// The single dimension of the first pick gives way to one between both picks. A point goes
// first: a distance to a line is measured from its first reference.
bool SketchEditorToolController::selectPair(const DimensionPick& first,
                                            const DimensionPick& second) {
    const bool swap = !first.point && second.point;
    const auto& from = swap ? second : first;
    const auto& to = swap ? first : second;
    return editor_->selectNearestReference(from.element, from.at, false) &&
           editor_->selectNearestReference(to.element, to.at, true);
}

std::optional<document::SketchConstraintId>
SketchEditorToolController::pairWithFirst(const DimensionPick& second) {
    if (dimensionAnchor_)
        static_cast<void>(
            editor_->removeConstraint(document::SketchConstraintId{*dimensionAnchor_}));
    dimensionAnchor_.reset();
    const auto first = *dimensionFirst_;
    dimensionFirst_.reset();
    if (!selectPair(first, second))
        return std::nullopt;
    const auto kind =
        pairDimension(*editor_, static_cast<int>(first.point) + static_cast<int>(second.point));
    if (!kind || !editor_->addSelectedRelation(*kind))
        return std::nullopt;
    return newestConstraint(*editor_);
}

// VP02: the distance of two points, in X, in Y or direct. A distance in X or Y counts from the
// lower point, so a typed size keeps the points on their sides.
void SketchEditorToolController::addPointDimension(document::SketchConstraintKind kind) {
    if (!editor_)
        return;
    auto references = editor_->selectedReferences();
    if (references.size() != 2)
        return;
    const auto first = editor_->referencePosition(references[0]);
    const auto second = editor_->referencePosition(references[1]);
    const bool across = kind == SketchConstraintKind::pointDistanceX;
    if (first && second && kind != SketchConstraintKind::pointDistance &&
        (across ? second->x < first->x : second->y < first->y)) {
        static_cast<void>(editor_->selectNearestReference(references[1].element, *second, false));
        static_cast<void>(editor_->selectNearestReference(references[0].element, *first, true));
    }
    finishPick(editor_->addSelectedRelation(kind) ? newestConstraint(*editor_) : std::nullopt,
               true);
}

// VP02: two points apart in both directions: which of the three distances is meant.
void SketchEditorToolController::askPointDimension(document::SketchPoint first,
                                                   document::SketchPoint second) {
    delete viewport_.findChild<QMenu*>(QStringLiteral("dimensionAxisMenu"));
    auto* menu = new QMenu(&viewport_);
    menu->setObjectName(QStringLiteral("dimensionAxisMenu"));
    const auto choice = [&](const QString& text, double value, SketchConstraintKind kind) {
        QObject::connect(menu->addAction(text.arg(value, 0, 'f', 2)), &QAction::triggered, menu,
                         [this, kind] { addPointDimension(kind); });
    };
    choice(QObject::tr("Waagerecht  %1 mm"), std::abs(second.x - first.x),
           SketchConstraintKind::pointDistanceX);
    choice(QObject::tr("Senkrecht  %1 mm"), std::abs(second.y - first.y),
           SketchConstraintKind::pointDistanceY);
    choice(QObject::tr("Direkt  %1 mm"), std::hypot(second.x - first.x, second.y - first.y),
           SketchConstraintKind::pointDistance);
    statusChanged_(QObject::tr("Welchen Abstand? Waagerecht, senkrecht oder direkt"));
    menu->popup(QCursor::pos());
}

void SketchEditorToolController::finishPick(std::optional<document::SketchConstraintId> created,
                                            bool pair) {
    editor_->clearSelection();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (!created) {
        dimensionAnchor_.reset();
        dimensionFirst_.reset();
        statusChanged_(QObject::tr("Für diese Auswahl gibt es kein Maß"));
        return;
    }
    statusChanged_(pair ? QObject::tr("Maß zwischen beiden: Wert tippen, Enter")
                        : QObject::tr("Wert tippen und Enter, oder ein zweites Element anklicken"));
    const auto angle = editor_->constraintAngle(*created);
    const double value = angle ? kernel::toDegrees(*angle).value()
                               : editor_->constraintValue(*created).value_or(0.0);
    openDimensionEdit(created->value(), QString::number(std::abs(value), 'g', 12));
}

// A first pick waits while it is a point or its measured dimension is not typed over; a second
// pick is another element, or another point of the same one.
bool SketchEditorToolController::pairsWithFirst(const DimensionPick& pick) const {
    if (!dimensionFirst_ || (!dimensionFirst_->point && !dimensionAnchor_))
        return false;
    return dimensionFirst_->element != pick.element ||
           (pick.point && dimensionFirst_->point &&
            std::hypot(dimensionFirst_->at.x - pick.at.x, dimensionFirst_->at.y - pick.at.y) >
                1.0e-9);
}

// Two points apart in both directions: ask, before any dimension exists.
bool SketchEditorToolController::askedForDistance(const DimensionPick& pick) {
    const auto first = *dimensionFirst_;
    if (!first.point || !pick.point || std::abs(first.at.x - pick.at.x) <= 1.0e-6 ||
        std::abs(first.at.y - pick.at.y) <= 1.0e-6 || !selectPair(first, pick))
        return false;
    dimensionFirst_.reset();
    askPointDimension(first.at, pick.at);
    refreshPresentation();
    return true;
}

bool SketchEditorToolController::pickForDimension(const viewport::SketchSelectionIntent& intent) {
    const document::SketchElementId element{intent.elementId};
    const auto point = intent.point
                           ? editor_->pointAt(element, {intent.point->first, intent.point->second})
                           : std::nullopt;
    const document::SketchPoint at =
        point ? editor_->referencePosition(*point).value_or(document::SketchPoint{})
              : document::SketchPoint{intent.localX, intent.localY};
    const DimensionPick pick{element, at, point.has_value()};
    if (pairsWithFirst(pick)) {
        if (!askedForDistance(pick))
            finishPick(pairWithFirst(pick), true);
        return true;
    }
    if (pick.point) {
        // VP02: the point stays marked until the second pick says what it is measured to.
        dimensionAnchor_.reset();
        dimensionFirst_ = pick;
        static_cast<void>(editor_->selectPick(element, at, at, false));
        refreshPresentation();
        statusChanged_(QObject::tr("Punkt gewählt: zweiten Punkt oder eine Linie anklicken"));
        return true;
    }
    const auto created = singleDimensionFor(element);
    dimensionFirst_ = pick;
    dimensionAnchor_ = created ? std::optional{created->value()} : std::nullopt;
    finishPick(created, false);
    return true;
}

} // namespace voelund::desktop
