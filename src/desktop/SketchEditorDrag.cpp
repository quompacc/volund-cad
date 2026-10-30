// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QObject>

// U09: moving the selection by pointer drag or typed offsets, as one user action.
namespace voelund::desktop {

// U09: moves from the state before the drag, so the panel totals are the drag's offset.
bool SketchEditorToolController::dragTo(double translationX, double translationY) {
    if (!editor_->isDragging()) {
        editor_->beginDrag();
        dragBaseX_ = translationX_;
        dragBaseY_ = translationY_;
    }
    if (!editor_->dragSelected(translationX - dragBaseX_, translationY - dragBaseY_))
        return false;
    translationX_ = translationX;
    translationY_ = translationY;
    return true;
}

void SketchEditorToolController::finishDrag() {
    if (!editor_ || !editor_->isDragging())
        return;
    editor_->endDrag();
    syncConstraintState(); // OK is locked while a drag runs
    refreshPresentation();
}

void SketchEditorToolController::translateFromViewport(double deltaX, double deltaY) {
    // W02: a click of the dimension tool picks; a small move while it is pressed moves nothing.
    if (toolIndex_ == dimensionTool)
        return;
    if (!editor_ || !dragTo(translationX_ + deltaX, translationY_ + deltaY))
        return;
    panel_.setLengthValue("translation_x", translationX_);
    panel_.setLengthValue("translation_y", translationY_);
    syncConstraintState();
    refreshPresentation();
    if (editor_->lastSolveResult().degreesOfFreedom == 0) {
        statusChanged_(QObject::tr("Alles ist festgelegt: Maß ändern oder Bedingung entfernen, "
                                   "um es zu verschieben"));
        return;
    }
    statusChanged_(QObject::tr("Skizzenelement verschoben: ΔX %1 mm, ΔY %2 mm")
                       .arg(translationX_, 0, 'f', 3)
                       .arg(translationY_, 0, 'f', 3));
}

void SketchEditorToolController::applyTranslation(const std::string& id, double value) {
    if (!editor_ || !editor_->selectedElement().has_value())
        return;
    const double x = id == "translation_x" ? value : translationX_;
    const double y = id == "translation_y" ? value : translationY_;
    if (dragTo(x, y)) {
        syncConstraintState();
        refreshPresentation();
    }
}

} // namespace voelund::desktop
