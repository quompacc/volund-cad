#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>

// U06: view state from the model tree (origins, planes, grid, body visibility) and picks of
// base planes from the tree or the viewport.
namespace voelund::desktop {

void MainWindow::syncVisibility() {
    const bool hasBodies = !impl_->session.displayBodies().empty();
    // M01: body visibility is stored with the document.
    impl_->visibility.hiddenBodies.clear();
    for (const auto& body : impl_->session.modelOutline().bodies)
        if (body.hidden)
            impl_->visibility.hiddenBodies.insert(body.id.value());
    impl_->viewport->setBodies(makeSceneBodies(impl_->session, impl_->visibility.hiddenBodies));
    impl_->viewport->setBasePlaneVisibility(impl_->visibility.visiblePlanes(hasBodies));
    impl_->viewport->setGridVisible(impl_->visibility.grid);
    syncViewSwitches();
}

// S13: four view switches as commands; none of them touches the document (no undo, no
// modified state). The grid also follows the model tree eye; the sketch switches mirror the
// sketch panel's choices.
void MainWindow::createViewSwitches() {
    impl_->gridAction = new QAction(tr("Raster anzeigen"), this);
    impl_->gridAction->setCheckable(true);
    impl_->gridAction->setChecked(impl_->visibility.grid);
    setCommandId(*impl_->gridAction, "view.grid");
    addAction(impl_->gridAction);
    connect(impl_->gridAction, &QAction::toggled, this, [this](bool on) {
        if (impl_->visibility.grid == on)
            return;
        impl_->visibility.grid = on;
        syncVisibility();
        syncModelTree();
    });
    impl_->sketchEditorTool->setGridSwitch([this] { return impl_->visibility.grid; },
                                           [this](bool on) { impl_->gridAction->setChecked(on); });
    const std::pair<const char*, QString> switches[] = {
        {"show_dimensions", tr("Bemaßungen anzeigen")},
        {"show_projected", tr("Projizierte Geometrie anzeigen")},
        {"relations", tr("Beziehungen automatisch setzen")}};
    for (const auto& [id, text] : switches) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        setCommandId(*action, std::string(id) == "relations"
                                  ? "sketch.auto_relations"
                                  : ("sketch." + std::string(id)).c_str());
        addAction(action);
        connect(action, &QAction::triggered, this, [this, key = std::string(id)](bool on) {
            impl_->sketchEditorTool->setViewSwitch(key, on);
        });
        impl_->sketchSwitchActions.emplace_back(id, action);
    }
    impl_->sketchEditorTool->setSwitchHandler([this] { syncViewSwitches(); });
    syncViewSwitches();
}

void MainWindow::syncViewSwitches() {
    if (impl_->gridAction == nullptr || !impl_->sketchEditorTool)
        return;
    {
        const QSignalBlocker blocker(impl_->gridAction);
        impl_->gridAction->setChecked(impl_->visibility.grid);
    }
    impl_->sketchEditorTool->syncGridChoice();
    const bool sketching = impl_->sketchEditorTool->isActive();
    for (const auto& [id, action] : impl_->sketchSwitchActions) {
        const QSignalBlocker blocker(action);
        action->setEnabled(sketching);
        action->setChecked(impl_->sketchEditorTool->viewSwitch(id));
    }
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
    case ModelTreePanel::Kind::body: {
        // M01: stored with the document and modified, without an undo step.
        const bool hidden = visibility.hiddenBodies.contains(id);
        static_cast<void>(impl_->session.setBodyHidden(document::BodyId{id}, !hidden));
        syncVisibility();
        syncModelTree();
        syncShell();
        return;
    }
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
    impl_->selectedBody.reset();
    impl_->selectedPlanarFace.reset();
    selectFeature(featureId);
    // Sketches open their editor, sketch extrusions their panel (M04); other features are
    // edited in the properties card.
    QAction* edit = impl_->commandActions->action("solid.modify.edit_sketch");
    if (impl_->session.selectedFeature() == document::FeatureId{featureId} && edit->isEnabled())
        edit->trigger();
}

} // namespace voelund::desktop
