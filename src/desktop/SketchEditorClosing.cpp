#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QMessageBox>
#include <QObject>
#include <QPushButton>

// S13: how the sketch editor is left (user decision 2026-10-09). Escape steps back (drag,
// preview points, drawing tool, selection) and never discards the sketch; Cancel asks before
// it discards changes; Enter ends a polyline, Ctrl+Enter or OK accepts.
namespace voelund::desktop {

void SketchEditorToolController::stepBack() {
    if (!editor_)
        return;
    // U09: Escape during a drag restores the state before it and keeps the editor open.
    if (editor_->isDragging()) {
        static_cast<void>(viewport_.cancelSketchElementDrag());
        editor_->cancelDrag();
        translationX_ = dragBaseX_;
        translationY_ = dragBaseY_;
        panel_.setLengthValue("translation_x", translationX_);
        panel_.setLengthValue("translation_y", translationY_);
        syncConstraintState();
        refreshPresentation();
        statusChanged_(QObject::tr("Verschieben abgebrochen"));
        return;
    }
    if (viewport_.cancelSketchEditorStep()) {
        editor_->clearPreview();
        refreshPresentation();
        statusChanged_(QObject::tr("Aktueller Skizzenschritt abgebrochen"));
        return;
    }
    // S13: then the drawing tool, then the selection; Escape never discards the sketch.
    if (toolIndex_ != 0) {
        panel_.chooseOption("tool", 0);
        statusChanged_(QObject::tr("Auswahlwerkzeug"));
        return;
    }
    if (editor_->selectedElement().has_value() || !editor_->selectedReferences().empty()) {
        clearSelection();
        statusChanged_(QObject::tr("Auswahl aufgehoben"));
        return;
    }
    statusChanged_(QObject::tr("Skizze mit OK (Strg+Enter) übernehmen oder mit Abbrechen "
                               "verwerfen"));
}

void SketchEditorToolController::requestCancel() {
    if (editor_ && editor_->canUndo()) {
        QMessageBox question(QMessageBox::Question, QObject::tr("Skizze abbrechen"),
                             QObject::tr("Änderungen an der Skizze verwerfen?"),
                             QMessageBox::NoButton, &panel_);
        question.setObjectName(QStringLiteral("discardSketchQuestion"));
        auto* discard = question.addButton(QObject::tr("Verwerfen"), QMessageBox::DestructiveRole);
        question.addButton(QObject::tr("Weiter bearbeiten"), QMessageBox::RejectRole);
        question.exec();
        if (question.clickedButton() != discard)
            return;
    }
    cancel();
}

void SketchEditorToolController::finishStep() {
    if (!editor_)
        return;
    if (toolIndex_ == 2 && viewport_.cancelSketchEditorStep()) {
        editor_->clearPreview();
        refreshPresentation();
        statusChanged_(QObject::tr("Polylinie beendet"));
        return;
    }
    statusChanged_(QObject::tr("Strg+Enter übernimmt die Skizze"));
}

void SketchEditorToolController::setGridSwitch(std::function<bool()> shown,
                                               std::function<void(bool)> show) {
    gridShown_ = std::move(shown);
    showGrid_ = std::move(show);
}

void SketchEditorToolController::syncGridChoice() {
    const int wanted = gridShown_ && gridShown_() ? 0 : 1;
    if (editor_ && panel_.choiceIndex("show_grid") != wanted)
        panel_.chooseOption("show_grid", wanted);
}

bool SketchEditorToolController::viewSwitch(const std::string& id) const {
    return id == "relations"         ? automaticRelations_
           : id == "show_dimensions" ? showDimensions_
           : id == "show_projected"  ? showProjected_
           : id == "show_grid"       ? gridShown_ && gridShown_()
                                     : false;
}

// From a command: through the panel choice, so panel and command stay alike.
void SketchEditorToolController::setViewSwitch(const std::string& id, bool on) {
    if (editor_)
        panel_.chooseOption(id, on ? 0 : 1);
    else
        applyViewSwitch(id, on);
}

void SketchEditorToolController::applyViewSwitch(const std::string& id, bool on) {
    if (id == "relations") {
        automaticRelations_ = on;
        viewport_.setSketchAutomaticRelations(on);
    } else if (id == "show_grid") {
        if (showGrid_)
            showGrid_(on);
    } else if (id == "show_dimensions") {
        showDimensions_ = on;
        refreshPresentation();
    } else if (id == "show_projected") {
        showProjected_ = on;
        viewport_.setSketchProjectedGeometryVisible(on);
    }
    if (switchChanged_)
        switchChanged_();
}

} // namespace voelund::desktop
