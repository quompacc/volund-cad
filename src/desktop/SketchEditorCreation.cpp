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
    if (!editor_)
        return;
    const auto circle = drawn(clicks);
    if (!circle) {
        statusChanged_(QObject::tr("Die drei Punkte liegen auf einer Linie"));
        return;
    }
    const viewport::SketchElementIntent& intent = *circle;
    if (toolIndex_ == bsplineTool) { // S21: each click adds a control point
        if (bsplinePoles_.empty())
            bsplinePoles_.push_back({intent.firstX, intent.firstY});
        bsplinePoles_.push_back({intent.secondX, intent.secondY});
        viewport_.continueSketchPolyline(intent.secondX, intent.secondY, 0);
        statusChanged_(
            QObject::tr("B-Spline: %1 Kontrollpunkte; Enter beendet").arg(bsplinePoles_.size()));
        return;
    }
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
