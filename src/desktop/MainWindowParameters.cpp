#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include <QComboBox>
#include <QLabel>
#include <QStatusBar>

namespace voelund::desktop {
using application::CommandStatus;
using application::ErrorDisposition;

void MainWindow::applyHistoryCommand(bool forward) {
    static_cast<void>(guardCommand(
        [&] {
            const auto result = forward ? impl_->session.redo() : impl_->session.undo();
            if (result.status() == CommandStatus::rejected)
                return false;
            refreshScene();
            if (result.status() == CommandStatus::committedWithErrors) {
                const auto message = commandFeedbackMessage(result);
                impl_->propertyPanel->showCommandError(message);
                statusBar()->showMessage(message);
            }
            return true;
        },
        [this](const QString& message) { statusBar()->showMessage(message); }));
}

void MainWindow::syncGeometryStatus() {
    const auto state = impl_->session.sceneStatus();
    impl_->geometryStatus->setVisible(!state.isCurrent());
    if (state.isCurrent()) {
        impl_->geometryStatus->clear();
        return;
    }
    impl_->geometryStatus->setText(
        state.geometry
            ? tr("Letzte gültige Geometrie r%1 — Modell r%2 hat Fehler")
                  .arg(state.geometry->revision)
                  .arg(state.document.revision)
            : tr("Keine gültige Geometrie — Modell r%1 hat Fehler").arg(state.document.revision));
}

void MainWindow::applyParameterCommand(
    std::function<application::CommandResult(ErrorDisposition)> command) {
    static_cast<void>(guardCommand(
        [&] {
            const auto result = command(ErrorDisposition::reject);
            if (result.status() == CommandStatus::committed) {
                refreshScene();
                statusBar()->showMessage(tr("Modell neu berechnet"), 2000);
                return true;
            }
            if (result.reason() == application::RejectionReason::noChange) {
                syncProperties();
                statusBar()->clearMessage();
                return false;
            }
            syncProperties();
            const auto message = commandFeedbackMessage(result);
            impl_->propertyPanel->showCommandError(message);
            statusBar()->showMessage(message);
            if (result.reason() == application::RejectionReason::evaluationFailed) {
                impl_->pendingParameter = command;
                impl_->pendingStamp = impl_->session.sceneStatus().document;
                impl_->pendingSelection = impl_->session.selectedFeature();
                impl_->propertyPanel->setErrorCommitHandler([this] { keepParameterError(); });
            }
            return false;
        },
        [this](const QString& message) {
            syncProperties();
            impl_->propertyPanel->showCommandError(message);
            statusBar()->showMessage(message);
        }));
}

void MainWindow::keepParameterError() {
    if (!impl_->pendingParameter)
        return;
    auto command = std::move(impl_->pendingParameter);
    const bool sameDocument = impl_->pendingStamp == impl_->session.sceneStatus().document &&
                              impl_->pendingSelection == impl_->session.selectedFeature();
    impl_->pendingStamp.reset();
    impl_->propertyPanel->setErrorCommitHandler({});
    if (!sameDocument) {
        syncProperties();
        impl_->propertyPanel->showCommandError(
            tr("Das Modell hat sich geändert. Gib die Änderung erneut ein."));
        return;
    }
    static_cast<void>(guardCommand(
        [&] {
            const auto result = command(ErrorDisposition::keepEditableErrors);
            if (result.status() == CommandStatus::committed ||
                result.status() == CommandStatus::committedWithErrors)
                refreshScene();
            const auto message = commandFeedbackMessage(result);
            if (result.status() != CommandStatus::committed) {
                impl_->propertyPanel->showCommandError(message);
                statusBar()->showMessage(message);
            }
            return result.status() != CommandStatus::rejected;
        },
        [this](const QString& message) {
            syncProperties();
            impl_->propertyPanel->showCommandError(message);
            statusBar()->showMessage(message);
        }));
}

void MainWindow::applyBoxParameters() {
    const auto parameters = impl_->propertyPanel->boxParameters();
    applyParameterCommand([this, parameters](ErrorDisposition disposition) {
        return impl_->session.editSelectedBoxParameters(parameters, disposition);
    });
}
void MainWindow::applyCylinderParameters() {
    const auto parameters = impl_->propertyPanel->cylinderParameters();
    applyParameterCommand([this, parameters](ErrorDisposition disposition) {
        return impl_->session.editSelectedCylinderParameters(parameters, disposition);
    });
}
void MainWindow::applyExtrusionParameters() {
    const auto parameters = impl_->propertyPanel->extrusionParameters();
    applyParameterCommand([this, parameters](ErrorDisposition disposition) {
        return impl_->session.editSelectedExtrusionParameters(parameters, disposition);
    });
}
} // namespace voelund::desktop
