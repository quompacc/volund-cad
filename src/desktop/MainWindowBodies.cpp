#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <QAction>
#include <QStatusBar>

// M01 (user decision 2026-10-10): bodies as a whole. A body row in the model tree selects the
// body; its name is edited in place (double-click, F2 or "Umbenennen") and is one undo step;
// the eye and "Ausblenden" store the visibility with the document without an undo step.
namespace voelund::desktop {

void MainWindow::createBodyCommands() {
    impl_->renameBodyAction = new QAction(tr("Umbenennen"), this);
    setCommandId(*impl_->renameBodyAction, "body.rename");
    addAction(impl_->renameBodyAction);
    connect(impl_->renameBodyAction, &QAction::triggered, this, [this] {
        if (impl_->selectedBody)
            impl_->modelTree->editBody(*impl_->selectedBody);
    });
    impl_->bodyVisibilityAction = new QAction(tr("Ausblenden"), this);
    setCommandId(*impl_->bodyVisibilityAction, "body.visibility");
    addAction(impl_->bodyVisibilityAction);
    connect(impl_->bodyVisibilityAction, &QAction::triggered, this, [this] {
        // N02b: a group shows or hides all its bodies.
        for (const auto body : selectedBodies())
            toggleVisibility(static_cast<int>(ModelTreePanel::Kind::body), body.value());
    });
    syncBodyCommands();
}

void MainWindow::syncBodyCommands() {
    if (impl_->renameBodyAction == nullptr)
        return;
    const ShellState shell = impl_->shell();
    const bool body = shell.selection == ShellSelection::body && !shell.toolBlocksCommands();
    impl_->renameBodyAction->setEnabled(body && impl_->bodyGroup.size() < 2); // N02b
    impl_->bodyVisibilityAction->setEnabled(body);
    impl_->bodyVisibilityAction->setText(shell.bodyHidden ? tr("Einblenden") : tr("Ausblenden"));
    syncClipboardCommands();
}

void MainWindow::selectBody(std::uint64_t id, bool keepGroup) {
    if (impl_->shell().toolBlocksCommands())
        return;
    if (!keepGroup)
        clearGroups();
    impl_->selectedDatum.reset();
    impl_->selectedPlanarFace.reset();
    // The creator feature stands for the body in the document selection, so a cylinder or a
    // move targets this body (CadSession::selectedBodyProducer).
    static_cast<void>(impl_->session.selectFeature(document::FeatureId{id}));
    impl_->selectedBody = document::BodyId{id};
    impl_->modelTree->selectBody(document::BodyId{id});
    syncTimeline();
    syncProperties();
    syncSketchPresentation();
    syncTransformTool();
    syncShell();
    statusBar()->showMessage(tr("%1 gewählt").arg(impl_->shell().bodyName), 2000);
}

void MainWindow::renameBody(std::uint64_t id, const QString& name) {
    if (!runCommand(
            [&] { return impl_->session.renameBody(document::BodyId{id}, name.toStdString()); },
            [this](const QString& message) { statusBar()->showMessage(message); })) {
        syncModelTree(); // the row shows the stored name again
        return;
    }
    refreshScene();
    selectBody(id);
    statusBar()->showMessage(tr("Körper umbenannt"), 2000);
}

} // namespace voelund::desktop
