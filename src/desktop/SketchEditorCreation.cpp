// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include "SketchCircleVariants.hpp"
#include "ToolPanelHost.hpp"

#include <QObject>

#include <algorithm>

namespace voelund::desktop {
namespace {

application::SketchSnapKind snapKind(viewport::SketchPointKind point) {
    switch (point) {
    case viewport::SketchPointKind::start:
        return application::SketchSnapKind::start;
    case viewport::SketchPointKind::end:
        return application::SketchSnapKind::end;
    case viewport::SketchPointKind::center:
        return application::SketchSnapKind::center;
    case viewport::SketchPointKind::midpoint:
        return application::SketchSnapKind::midpoint;
    case viewport::SketchPointKind::origin:
        return application::SketchSnapKind::origin;
    case viewport::SketchPointKind::xAxis:
        return application::SketchSnapKind::xAxis;
    case viewport::SketchPointKind::yAxis:
        return application::SketchSnapKind::yAxis;
    case viewport::SketchPointKind::projected:
        return application::SketchSnapKind::projected;
    case viewport::SketchPointKind::onObject:
        break;
    }
    return application::SketchSnapKind::onObject;
}

std::optional<application::SketchSnap>
snap(const std::optional<viewport::SketchPointReferenceIntent>& reference) {
    if (!reference.has_value())
        return std::nullopt;
    return application::SketchSnap{snapKind(reference->point),
                                   document::SketchElementId{reference->elementId},
                                   reference->supportReferenceId,
                                   reference->supportReferenceParameter, reference->projectedEdge};
}

// Parallel to viewport::SketchAutomaticConstraint.
application::SketchAutoRelation relation(viewport::SketchAutomaticConstraint constraint) {
    return static_cast<application::SketchAutoRelation>(constraint);
}

// U08: the viewport's intent as the application's creation input; nothing else is decided
// here.
application::SketchCreationInput creationInput(const viewport::SketchElementIntent& intent,
                                               bool centered, bool automaticRelations,
                                               bool construction) {
    application::SketchCreationInput input{{intent.firstX, intent.firstY},
                                           {intent.secondX, intent.secondY}};
    if (intent.third)
        input.third = document::SketchPoint{intent.third->first, intent.third->second};
    input.firstSnap = snap(intent.firstReference);
    input.secondSnap = snap(intent.secondReference);
    input.thirdSnap = snap(intent.thirdReference);
    if (intent.automaticConstraint)
        input.relation = relation(*intent.automaticConstraint);
    if (intent.automaticTargetId)
        input.relationTarget = document::SketchElementId{*intent.automaticTargetId};
    input.centered = centered;
    input.automaticRelations = automaticRelations;
    input.construction = construction; // W20
    return input;
}

} // namespace

// W20: a two- or three-point circle is made as the circle through the clicks.
std::optional<viewport::SketchElementIntent>
SketchEditorToolController::drawn(const viewport::SketchElementIntent& clicks) const {
    if (!circleVariant())
        return clicks;
    return circleThrough(clicks, toolIndex_ == circleThreePointTool);
}

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& clicks) {
    previewAutomatic_.reset();
    if (!editor_ || confirmOwnTool(clicks))
        return;
    const auto circle = drawn(clicks);
    if (!circle) {
        statusChanged_(QObject::tr("Die drei Punkte liegen auf einer Linie"));
        return;
    }
    const viewport::SketchElementIntent& intent = *circle;
    std::optional<application::SketchCreationResult> created;
    try {
        // A rejected step leaves the editor unchanged (S02).
        created = editor_->create(creationInput(intent, toolIndex_ == centerRectangleTool,
                                                automaticRelations_, drawConstruction_));
    } catch (const std::invalid_argument& error) {
        panel_.setAcceptEnabled(editor_->canAccept());
        refreshPresentation();
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    if (created && !intent.typed.empty())
        applyTypedDimensions(*created, intent.typed); // D01: one undo step with the element
    if (created)
        applyRectangleOptions(*created, intent); // W13: in the same step
    panel_.setAcceptEnabled(editor_->canAccept());
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (!created)
        return;
    continuePolyline(*created);
    statusChanged_(QObject::tr("Skizzenelement #%1 hinzugefügt").arg(created->primary().value()));
}

// The tools that make their elements themselves; false for the others.
bool SketchEditorToolController::confirmOwnTool(const viewport::SketchElementIntent& clicks) {
    if (toolIndex_ == bsplineTool)
        collectBsplinePole(clicks);
    else if (toolIndex_ == threePointRectangleTool)
        confirmThreePointRectangle(clicks); // W20
    else if (toolIndex_ == tangentArcTool)
        confirmTangentArc(clicks); // W20
    else
        return false;
    return true;
}

// S21: each click adds a control point; Enter makes the spline.
void SketchEditorToolController::collectBsplinePole(const viewport::SketchElementIntent& clicks) {
    if (bsplinePoles_.empty())
        bsplinePoles_.push_back({clicks.firstX, clicks.firstY});
    bsplinePoles_.push_back({clicks.secondX, clicks.secondY});
    viewport_.continueSketchPolyline(clicks.secondX, clicks.secondY, 0);
    statusChanged_(
        QObject::tr("B-Spline: %1 Kontrollpunkte; Enter beendet").arg(bsplinePoles_.size()));
}

// W20: the rectangle on the first two clicks' edge, as wide as the third click lies off it.
void SketchEditorToolController::confirmThreePointRectangle(
    const viewport::SketchElementIntent& clicks) {
    if (!clicks.third)
        return;
    try {
        static_cast<void>(editor_->createThreePointRectangle(
            {clicks.firstX, clicks.firstY}, {clicks.secondX, clicks.secondY},
            {clicks.third->first, clicks.third->second}));
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Rechteck aus 3 Punkten hinzugefügt"));
}

// W20 (as Fusion): the first click took the end of a line, the arc goes on from it without a kink.
void SketchEditorToolController::confirmTangentArc(const viewport::SketchElementIntent& clicks) {
    const auto& start = clicks.firstReference;
    const bool lineEnd = start && (start->point == viewport::SketchPointKind::start ||
                                   start->point == viewport::SketchPointKind::end);
    try {
        if (!lineEnd)
            throw std::invalid_argument("Tangentialbogen: zuerst auf das Ende einer Linie klicken");
        static_cast<void>(editor_->createTangentArc(
            {document::SketchElementId{start->elementId},
             start->point == viewport::SketchPointKind::start ? document::SketchPointPosition::start
                                                              : document::SketchPointPosition::end},
            {clicks.secondX, clicks.secondY}));
    } catch (const std::invalid_argument& error) {
        refreshPresentation();
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Tangentialbogen hinzugefügt"));
}

// The polyline goes on from the end of the line just placed.
void SketchEditorToolController::continuePolyline(
    const application::SketchCreationResult& created) {
    const auto end = created.point(application::SketchCreationPoint::lineEnd);
    if (editor_->tool() != application::SketchDrawingTool::polyline || !end)
        return;
    const auto match =
        std::ranges::find(editor_->elements(), end->element, &document::SketchElement::id);
    if (match == editor_->elements().end())
        return;
    const auto& line = std::get<document::SketchLine>(match->geometry);
    viewport_.continueSketchPolyline(line.end.x, line.end.y, end->element.value());
}

} // namespace voelund::desktop
