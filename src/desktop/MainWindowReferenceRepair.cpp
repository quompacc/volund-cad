#include "DocumentRepairDialog.hpp"
#include "MainWindowState.hpp"
#include "ReferenceRepairDialog.hpp"
#include <QAction>
#include <QStatusBar>

namespace voelund::desktop {
void MainWindow::repairAllReferences() {
    if (impl_->sketchEditorTool->isActive())
        impl_->sketchEditorTool->cancel();
    if (impl_->sketchExtrusionTool->isActive())
        impl_->sketchExtrusionTool->cancel();
    auto* dialog = new DocumentRepairDialog(impl_->session, [this] { refreshScene(); }, this);
    dialog->open();
}
void MainWindow::repairReferences() {
    if (!impl_->session.selectedFeature() ||
        impl_->session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch)
        return;
    if (impl_->sketchEditorTool->isActive())
        impl_->sketchEditorTool->cancel();
    if (impl_->sketchExtrusionTool->isActive())
        impl_->sketchExtrusionTool->cancel();
    auto* dialog = new ReferenceRepairDialog(
        impl_->session, *impl_->session.selectedFeature(),
        [this] {
            refreshScene();
            statusBar()->showMessage(
                impl_->session.sceneStatus().isCurrent()
                    ? tr("Referenz repariert")
                    : tr("Reparatur behalten. Die übrigen Modellfehler brauchen Aufmerksamkeit."));
        },
        this);
    dialog->open();
}
} // namespace voelund::desktop
