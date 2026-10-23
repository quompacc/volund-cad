// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"
#include "OverlayCard.hpp"
#include "SectionController.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>

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

namespace {
viewport::SectionPlane sectionPlane(const document::SketchPlane& frame) {
    return {{frame.originX, frame.originY, frame.originZ},
            {frame.normalX, frame.normalY, frame.normalZ}};
}
} // namespace

// N05: the section view; its plane comes from the base planes, the construction planes or the
// selection. View state only.
void MainWindow::createSectionView() {
    SectionController::Hooks hooks;
    hooks.constructionPlanes = [this] {
        std::vector<SectionController::Choice> planes;
        for (const auto& feature : impl_->session.features())
            if (feature.kind == document::FeatureKind::datumPlane)
                if (const auto frame = impl_->session.datumFrame(feature.id))
                    planes.push_back({QString::fromStdString(feature.name), sectionPlane(*frame)});
        return planes;
    };
    hooks.selectedPlane = [this]() -> std::optional<SectionController::Choice> {
        if (const auto& face = impl_->selectedPlanarFace)
            return SectionController::Choice{tr("Gewählte Fläche"),
                                             {{face->originX, face->originY, face->originZ},
                                              {face->normalX, face->normalY, face->normalZ}}};
        if (impl_->selectedDatum)
            return SectionController::Choice{
                tr("Gewählte Ebene"),
                sectionPlane(document::datumSketchPlane(*impl_->selectedDatum))};
        const auto selected = impl_->session.selectedFeature();
        for (const auto& feature : impl_->session.features())
            if (feature.id == selected && feature.kind == document::FeatureKind::datumPlane)
                if (const auto frame = impl_->session.datumFrame(feature.id))
                    return SectionController::Choice{QString::fromStdString(feature.name),
                                                     sectionPlane(*frame)};
        return std::nullopt;
    };
    hooks.placed = [this] {
        impl_->modelCard->place();
        if (impl_->parameterCard != nullptr)
            impl_->parameterCard->place();
        impl_->section->card().place();
    };
    impl_->section = new SectionController(*this, *impl_->viewport, std::move(hooks));
    impl_->header->setSectionAction(impl_->section->action());
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
    impl_->timeline->setReorderHandler( // M16
        [this](std::uint64_t id, std::optional<std::uint64_t> after) {
            moveFeatureAfter(document::FeatureId{id},
                             after ? std::optional{document::FeatureId{*after}} : std::nullopt);
        });
    impl_->timeline->setDeleteHandler([this](std::uint64_t id) {
        if (impl_->shell().toolBlocksCommands())
            return;
        selectFeature(id);
        syncShell();
        deleteSelectedFeature();
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
            icons.emplace_back(
                feature.id,
                shapes[static_cast<std::size_t>(snapshot.primitiveParameters(feature.id).shape)]);
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
    // W08 (as in Fusion): the menu starts with editing the step.
    for (auto* action : findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String("solid.modify.edit_sketch")) {
            menu.addAction(action);
            menu.addSeparator();
            break;
        }
    menu.addAction(impl_->suppressAction);
    menu.addAction(impl_->rollbackAction);
    menu.addAction(impl_->rollForwardAction);
    menu.addSeparator();
    menu.addAction(impl_->deleteAction);
    menu.exec(at);
}

// W09 (as in Fusion and SolidCraft): a tree row's menu. The row is selected first, like a click,
// so the menu offers the same commands (and keys) the bar and the search show for it.
void MainWindow::showTreeMenu(int kind, std::uint64_t id, QPoint at) {
    if (impl_->shell().toolBlocksCommands())
        return;
    std::vector<const char*> commands;
    switch (static_cast<ModelTreePanel::Kind>(kind)) {
    case ModelTreePanel::Kind::body:
        selectBody(id);
        commands = {"body.rename",         "body.visibility",
                    "edit.copy",           "solid.modify.transform",
                    "solid.modify.fillet", "solid.modify.mirror",
                    "document.export_step"};
        break;
    case ModelTreePanel::Kind::feature:
        selectFeature(id);
        commands = {"solid.modify.edit_sketch", "history.suppress", "history.rollback_here",
                    "history.delete"};
        break;
    case ModelTreePanel::Kind::plane:
        selectBasePlane(static_cast<int>(id));
        commands = {"solid.create.sketch", "solid.create.datum_plane"};
        break;
    default:
        return;
    }
    syncShell();
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("treeMenu"));
    const auto actions = findChildren<QAction*>();
    for (const char* command : commands)
        for (auto* action : actions)
            if (action->property("commandId").toString() == QLatin1String(command)) {
                menu.addAction(action);
                break;
            }
    if (!menu.isEmpty())
        menu.exec(at);
}

// M14: suppresses the selected feature, or reactivates it; one undo step.
void MainWindow::toggleSuppression() {
    const auto selected = impl_->session.selectedFeature();
    if (!selected || impl_->shell().toolBlocksCommands())
        return;
    if (impl_->featureGroup.size() > 1) // N02b: the whole group, one undo step
        return suppressGroup();
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

// M16 (user decision 2026-10-16): without dependents the feature goes at once; otherwise a
// question names them and offers the cascade, the feature alone (they stay with errors) or
// nothing. One undo step either way.
// M16: asks what deleting `subject` takes along; cancel, the dependents too, or it alone.
MainWindow::DeletionChoice MainWindow::askDeletion(const QString& subject,
                                                   const application::DeletionPlan& plan) {
    const auto features = impl_->session.features();
    QStringList names;
    for (const auto id : plan.dependents) {
        const auto found = std::ranges::find(features, id, &document::FeatureDescriptor::id);
        names << QStringLiteral("• ") +
                     (found == features.end() ? QString{} : QString::fromStdString(found->name));
    }
    QMessageBox question(QMessageBox::Question, tr("Feature löschen"),
                         tr("%1 wird von diesen Features gebraucht, oder sie schlagen ohne "
                            "es fehl:\n%2")
                             .arg(subject, names.join(QLatin1Char('\n'))),
                         QMessageBox::NoButton, this);
    question.setObjectName(QStringLiteral("deleteFeatureQuestion"));
    auto* cascade = question.addButton(tr("Mit Abhängigen löschen"), QMessageBox::AcceptRole);
    auto* alone = question.addButton(tr("Nur dieses löschen"), QMessageBox::DestructiveRole);
    alone->setEnabled(plan.onlyThisPossible);
    if (!plan.onlyThisPossible)
        alone->setToolTip(tr("Die Abhängigen können ohne dieses Feature nicht bestehen."));
    question.addButton(tr("Abbrechen"), QMessageBox::RejectRole);
    question.exec();
    if (question.clickedButton() == cascade)
        return DeletionChoice::cascade;
    if (question.clickedButton() == alone && plan.onlyThisPossible)
        return DeletionChoice::alone;
    return DeletionChoice::cancel;
}

void MainWindow::deleteSelectedFeature() {
    const auto selected = impl_->session.selectedFeature();
    if (!selected || impl_->shell().toolBlocksCommands())
        return;
    if (chosenSteps().size() > 1) // N02b: a group of steps or bodies
        return deleteGroup();
    const auto plan = impl_->session.deletionPlan(*selected);
    const auto features = impl_->session.features();
    const auto named = std::ranges::find(features, *selected, &document::FeatureDescriptor::id);
    deleteSteps({*selected}, plan,
                QStringLiteral("„%1“").arg(
                    named == features.end() ? QString{} : QString::fromStdString(named->name)));
}

void MainWindow::deleteSteps(const std::vector<document::FeatureId>& chosen,
                             const application::DeletionPlan& plan, const QString& subject) {
    std::vector<document::FeatureId> doomed = chosen;
    auto disposition = application::ErrorDisposition::reject;
    if (!plan.dependents.empty()) {
        const auto choice = askDeletion(subject, plan);
        if (choice == DeletionChoice::cancel)
            return;
        if (choice == DeletionChoice::cascade)
            doomed = plan.cascade;
        else
            disposition = application::ErrorDisposition::keepEditableErrors;
    }
    if (!runCommand([&] { return impl_->session.deleteFeatures(doomed, disposition); },
                    [this](const QString& message) { statusBar()->showMessage(message); }))
        return;
    refreshScene();
    statusBar()->showMessage(tr("%n Feature(s) gelöscht", nullptr, static_cast<int>(doomed.size())),
                             2000);
}

// M16: refused before anything changes when the feature would stand before what it needs or
// when a feature would fail; the reason is shown.
void MainWindow::moveFeatureAfter(document::FeatureId feature,
                                  std::optional<document::FeatureId> after) {
    if (impl_->shell().toolBlocksCommands())
        return;
    const auto result = impl_->session.moveFeature(feature, after);
    using application::CommandStatus;
    if (result.status() == CommandStatus::committed) {
        refreshScene();
        statusBar()->showMessage(tr("Feature verschoben"), 2000);
        return;
    }
    if (result.status() == CommandStatus::rejected &&
        result.reason() == application::RejectionReason::noChange)
        return;
    const auto message =
        result.reason() == application::RejectionReason::invalidInput
            ? tr("Dort kann das Feature nicht stehen: Es käme vor etwas, das es braucht (etwa eine "
                 "Extrusion vor ihre Skizze oder eine Verrundung vor ihren Körper).")
            : tr("Verschieben abgelehnt: %1").arg(commandFeedbackMessage(result));
    statusBar()->showMessage(message, 6000);
    syncTimeline();
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
