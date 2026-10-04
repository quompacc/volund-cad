#include "CommandFeedback.hpp"
#include "SketchEditorToolController.hpp"
#include "ToolPanelHost.hpp"
#include <voelund/application/CadSession.hpp>

namespace voelund::desktop {
void SketchEditorToolController::accept(application::ErrorDisposition disposition) {
    if (!editor_ || !editor_->canAccept())
        return;
    panel_.setErrorCommitHandler({});
    static_cast<void>(guardCommand(
        [&] {
            const auto result = editor_->tryAccept(disposition);
            if (result.status() == application::CommandStatus::rejected &&
                result.reason() != application::RejectionReason::noChange) {
                const auto message = commandFeedbackMessage(result);
                panel_.setGuidance(message);
                statusChanged_(message);
                if (editing_ && result.reason() == application::RejectionReason::evaluationFailed) {
                    const auto stamp = session_.sceneStatus().document;
                    const auto draft = editor_->draftParameters();
                    panel_.setErrorCommitHandler([this, stamp, draft] {
                        if (!editor_ || session_.sceneStatus().document != stamp ||
                            editor_->draftParameters() != draft) {
                            panel_.setErrorCommitHandler({});
                            panel_.setGuidance(QObject::tr(
                                "Skizze oder Modell haben sich geändert. Bestätige erneut."));
                            return;
                        }
                        accept(application::ErrorDisposition::keepEditableErrors);
                    });
                }
                return false;
            }
            const bool wasEditing = editing_;
            editor_.reset();
            editing_ = false;
            viewport_.cancelSketch();
            panel_.end();
            sceneChanged_();
            statusChanged_(result.status() == application::CommandStatus::committedWithErrors
                               ? QObject::tr("Skizze mit bearbeitbaren Fehlern behalten")
                           : wasEditing ? QObject::tr("Skizze aktualisiert")
                                        : QObject::tr("Skizze angelegt"));
            return true;
        },
        [this](const QString& message) {
            panel_.setGuidance(message);
            statusChanged_(message);
        }));
}
} // namespace voelund::desktop
