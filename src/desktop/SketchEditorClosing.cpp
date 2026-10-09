#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QMessageBox>
#include <QObject>
#include <QPushButton>

// S13: how the sketch editor is left (user decision 2026-10-09). Escape steps back (drag,
// preview points, drawing tool, selection) and never discards the sketch; Cancel asks before
// it discards changes; Enter ends a polyline, Ctrl+Enter or OK accepts.
namespace voelund::desktop {

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

} // namespace voelund::desktop
