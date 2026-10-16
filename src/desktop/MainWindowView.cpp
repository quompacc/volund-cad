#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include <QAction>
#include <QMenu>
#include <QSignalBlocker>
#include <QStatusBar>

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
    impl_->viewport->setDatums(makeSceneDatums(impl_->session)); // M19
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
    clearEdgeSelection();
    impl_->selectedPlanarFace.reset();
    impl_->selectedDatum = static_cast<document::DatumPlane>(plane);
    static const char* const names[]{"XY", "XZ", "YZ"};
    statusBar()->showMessage(tr("%1-Ebene gewählt").arg(QString::fromLatin1(names[plane - 1])),
                             2000);
    syncShell();
}

void MainWindow::createTimeline() {
    impl_->timeline = new TimelineBar(*impl_->viewport);
    impl_->timeline->setOpenHandler([this](std::uint64_t id) { openTimelineStep(id); });
    impl_->timeline->setMenuHandler(
        [this](std::uint64_t id, QPoint at) { showTimelineMenu(id, at); }); // M14
    impl_->timeline->setRollbackHandler([this](std::optional<std::uint64_t> after) {
        moveRollback(after ? std::optional{document::FeatureId{*after}} : std::nullopt);
    });
    impl_->timeline->setSelectHandler([this](std::uint64_t id) { // M15: keyboard
        if (impl_->shell().toolBlocksCommands())
            return;
        selectFeature(id);
        syncShell();
    });
    syncTimeline();
}

void MainWindow::syncTimeline() {
    if (impl_->timeline == nullptr)
        return;
    const auto features = impl_->session.features();
    // M20/U14: revolves, helixes, sweeps, lofts, primitives and circular patterns (M10) show
    // their own symbol.
    std::vector<TimelineBar::IconOverride> icons;
    const auto snapshot = impl_->session.identifiedSnapshot().snapshot();
    for (const auto& feature : features)
        if (feature.kind == document::FeatureKind::extrusion) {
            const auto& extrusion = snapshot.extrusionParameters(feature.id);
            if (extrusion.revolve)
                icons.emplace_back(feature.id, extrusion.revolve->pitch.has_value()
                                                   ? ShellIcon::helix
                                                   : ShellIcon::revolve);
            else if (extrusion.sweep)
                icons.emplace_back(feature.id, ShellIcon::sweep);
            else if (extrusion.loft)
                icons.emplace_back(feature.id, ShellIcon::loft);
        } else if (feature.kind == document::FeatureKind::primitive) {
            static constexpr ShellIcon shapes[]{ShellIcon::sphere, ShellIcon::cone,
                                                ShellIcon::torus, ShellIcon::wedge};
            icons.emplace_back(feature.id, shapes[static_cast<std::size_t>(
                                               snapshot.primitiveParameters(feature.id).shape)]);
        } else if (feature.kind == document::FeatureKind::pattern &&
                   snapshot.patternParameters(feature.id).kind ==
                       document::PatternKind::circular) { // M10
            icons.emplace_back(feature.id, ShellIcon::patternCircular);
        }
    impl_->timeline->showFeatures(features, impl_->session.selectedFeature(), icons);
    impl_->timeline->showStates(impl_->session.featureStates());
}

// M14: the step's menu acts on that feature; it is selected first, like a click.
void MainWindow::showTimelineMenu(std::uint64_t featureId, QPoint at) {
    if (impl_->shell().toolBlocksCommands())
        return;
    selectFeature(featureId);
    syncShell();
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("timelineMenu"));
    menu.addAction(impl_->suppressAction);
    menu.addAction(impl_->rollbackAction);
    menu.addAction(impl_->rollForwardAction);
    menu.exec(at);
}

// M14: suppresses the selected feature, or reactivates it; one undo step.
void MainWindow::toggleSuppression() {
    const auto selected = impl_->session.selectedFeature();
    if (!selected || impl_->shell().toolBlocksCommands())
        return;
    const auto states = impl_->session.featureStates();
    const auto state = std::ranges::find(states, *selected, &application::FeatureRuntimeState::id);
    const bool suppress =
        state == states.end() || state->status != application::FeatureRuntimeStatus::suppressed;
    if (!runCommand(
            [&] {
                return runAction(impl_->session,
                                 application::SetFeatureSuppressedAction{*selected, suppress});
            },
            [this](const QString& message) { statusBar()->showMessage(message); }))
        return;
    refreshScene();
    statusBar()->showMessage(suppress ? tr("Feature unterdrückt") : tr("Feature aktiviert"), 2000);
}

// M14: the marker moves without an undo step and without an unsaved change.
void MainWindow::moveRollback(std::optional<document::FeatureId> after) {
    if (impl_->shell().toolBlocksCommands())
        return;
    if (!impl_->session.setRollback(after))
        return;
    refreshScene();
    statusBar()->showMessage(after ? tr("Zurückgerollt") : tr("Historie bis zum Ende berechnet"),
                             2000);
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
