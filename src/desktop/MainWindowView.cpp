#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QStatusBar>
#include <QToolBar>

// U06: view state from the model tree (origins, planes, grid, body visibility) and picks of
// base planes from the tree or the viewport.
namespace voelund::desktop {

void MainWindow::syncVisibility() {
    const bool hasBodies = !impl_->session.displayBodies().empty();
    impl_->viewport->setBodies(makeSceneBodies(impl_->session, impl_->visibility.hiddenBodies));
    impl_->viewport->setBasePlaneVisibility(impl_->visibility.visiblePlanes(hasBodies));
    impl_->viewport->setGridVisible(impl_->visibility.grid);
}

void MainWindow::toggleVisibility(int kind, std::uint64_t id) {
    ViewVisibility& visibility = impl_->visibility;
    const bool hasBodies = !impl_->session.displayBodies().empty();
    const auto toggle = [](std::set<std::uint64_t>& set, std::uint64_t value) {
        if (!set.erase(value))
            set.insert(value);
    };
    switch (static_cast<ModelTreePanel::Kind>(kind)) {
    case ModelTreePanel::Kind::origin:
        visibility.origin = !visibility.originShown(hasBodies);
        break;
    case ModelTreePanel::Kind::plane:
        if (id >= 1 && id <= 3)
            visibility.planes[id - 1] = !visibility.planes[id - 1];
        break;
    case ModelTreePanel::Kind::grid:
        visibility.grid = !visibility.grid;
        break;
    case ModelTreePanel::Kind::body:
        toggle(visibility.hiddenBodies, id);
        break;
    case ModelTreePanel::Kind::bodyOrigin:
        toggle(visibility.shownBodyOrigins, id);
        break;
    case ModelTreePanel::Kind::feature:
        return;
    }
    // View state only: no undo step and no change to the saved state.
    syncVisibility();
    syncModelTree();
}

void MainWindow::selectBasePlane(int plane) {
    if (plane < 1 || plane > 3)
        return;
    impl_->selectedPlanarFace.reset();
    impl_->selectedDatum = static_cast<document::DatumPlane>(plane);
    static const char* const names[]{"XY", "XZ", "YZ"};
    statusBar()->showMessage(tr("%1-Ebene gewählt").arg(QString::fromLatin1(names[plane - 1])),
                             2000);
    syncShell();
}

void MainWindow::createTimeline() {
    impl_->timeline = new TimelineBar(this);
    impl_->timeline->setOpenHandler([this](std::uint64_t id) { openTimelineStep(id); });
    auto* bar = new QToolBar(tr("Timeline"), this);
    bar->setObjectName(QStringLiteral("timelineToolbar"));
    bar->setMovable(false);
    bar->addWidget(impl_->timeline);
    addToolBar(Qt::BottomToolBarArea, bar);
    syncTimeline();
}

void MainWindow::syncTimeline() {
    if (impl_->timeline == nullptr)
        return;
    const auto features = impl_->session.features();
    impl_->timeline->showFeatures(features, impl_->session.selectedFeature());
    impl_->timeline->showStates(impl_->session.featureStates());
}

void MainWindow::openTimelineStep(std::uint64_t featureId) {
    if (impl_->shell().toolBlocksCommands()) {
        statusBar()->showMessage(tr("Schließe zuerst das offene Werkzeug"), 3000);
        syncTimeline();
        return;
    }
    impl_->selectedDatum.reset();
    impl_->selectedPlanarFace.reset();
    selectFeature(featureId);
    // Sketches open their editor; other features are edited in the properties card.
    QAction* edit = impl_->commandActions->action("solid.modify.edit_sketch");
    if (impl_->session.selectedFeature() == document::FeatureId{featureId} && edit->isEnabled())
        edit->trigger();
}

} // namespace voelund::desktop
