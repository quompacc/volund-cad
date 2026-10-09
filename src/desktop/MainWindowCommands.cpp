#include <voelund/desktop/MainWindow.hpp>

#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"

#include "ShellActions.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMetaObject>
#include <QSettings>
#include <QStatusBar>

#include <memory>
#include <utility>

// U04: commands and their surfaces (tool rail, context bar, command search; ADR 0012).
namespace voelund::desktop {

void MainWindow::createCommands() {
    modules::solid::SolidCommandServices services;
    services.addBox = [this]() {
        if (!runCommand(
                [this] {
                    return runAction(impl_->session,
                                     application::CreateBoxAction{
                                         millimetres(20.0), millimetres(20.0), millimetres(40.0)});
                },
                        [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Quader angelegt"), 2000);
    };
    services.addCylinder = [this]() {
        if (!runCommand(
                [this] {
                    // The request names the input the command offered: the last feature.
                    const auto features = impl_->session.features();
                    if (features.empty())
                        return application::CommandResult::invalidInput(
                            "A cylinder requires a current input feature");
                    return runAction(impl_->session,
                                     application::CreateCylinderAction{
                                         features.back().id, document::CylinderResultMode::join});
                },
                [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Zylinder angelegt"), 2000);
    };
    services.sketchEditor = [this]() {
        if (impl_->selectedDatum.has_value()) {
            if (impl_->sketchExtrusionTool->isActive())
                impl_->sketchExtrusionTool->cancel();
            impl_->sketchEditorTool->beginOnBasePlane(*impl_->selectedDatum);
            return;
        }
        if (impl_->selectedPlanarFace.has_value()) {
            if (impl_->sketchExtrusionTool->isActive()) {
                impl_->sketchExtrusionTool->cancel();
            }
            impl_->sketchEditorTool->begin(*impl_->selectedPlanarFace);
        }
    };
    services.editSketch = [this]() {
        if (impl_->sketchExtrusionTool->isActive()) {
            impl_->sketchExtrusionTool->cancel();
        }
        static_cast<void>(impl_->sketchEditorTool->beginSelectedSketch());
    };
    services.extrudeProfiles = [this]() {
        if (impl_->sketchEditorTool->isActive())
            impl_->sketchEditorTool->cancel();
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->sketchExtrusionTool->beginSelectedProfiles();
    };
    services.transform = [this](bool enabled) {
        impl_->viewport->setTransformToolEnabled(enabled);
        syncShell(); // The context bar switches to the snap settings.
    };
    services.canAddCylinder = [this]() {
        return impl_->session.canAddCylinder() && !impl_->shell().toolBlocksCommands();
    };
    services.canAddBox = [this]() { return !impl_->shell().toolBlocksCommands(); };
    // Enables "Sketch": a picked planar face or a picked base plane (V01b).
    services.canEditSketch = [this]() { return impl_->shell().canSketch(); };
    services.canEditExistingSketch = [this]() { return impl_->shell().canEditSketch(); };
    services.canExtrudeProfiles = [this]() { return impl_->shell().canExtrude(); };
    // T04c: push/pull of the picked planar face.
    services.pushPull = [this]() {
        if (!impl_->selectedPlanarFace)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->pushPullTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    services.canPushPull = [this]() {
        return impl_->selectedPlanarFace.has_value() && impl_->shell().canSketch();
    };
    services.canTransform = [this]() {
        const auto selected = impl_->session.selectedFeature();
        return selected.has_value() && impl_->session.canTransformFeature(*selected) &&
               !impl_->shell().toolBlocksCommands();
    };
    impl_->commandRegistry.registerModule(
        std::make_unique<modules::solid::SolidWorkspaceModule>(std::move(services)));
    impl_->commandActions =
        std::make_unique<CommandActions>(*this, impl_->commandRegistry.module("solid"));
    impl_->addBoxAction = impl_->commandActions->action("solid.create.box");
    impl_->addCylinderAction = impl_->commandActions->action("solid.create.cylinder");
    impl_->addCylinderAction->setObjectName(QStringLiteral("addCylinderAction"));
    impl_->sketchEditorAction = impl_->commandActions->action("solid.create.sketch");
    impl_->sketchEditorAction->setObjectName(QStringLiteral("sketchEditorAction"));
    impl_->addBoxAction->setObjectName(QStringLiteral("addBoxAction"));
    impl_->transformAction = impl_->commandActions->action("solid.modify.transform");
    impl_->transformAction->setObjectName(QStringLiteral("transformAction"));
}

void MainWindow::createCommandSurfaces() {
    impl_->searchAction = new QAction(tr("Befehl oder Werkzeug suchen"), this);
    impl_->searchAction->setIconText(tr("Befehl oder Werkzeug suchen      Strg K"));
    impl_->searchAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    setCommandId(*impl_->searchAction, "view.command_search");
    addAction(impl_->searchAction);
    impl_->toolRailAction = new QAction(tr("Werkzeugleiste"), this);
    impl_->toolRailAction->setCheckable(true);
    impl_->toolRailAction->setChecked(
        QSettings().value(QStringLiteral("ui/toolRail"), true).toBool());
    setCommandId(*impl_->toolRailAction, "view.tool_rail");
    addAction(impl_->toolRailAction);
    impl_->header->setViewActions(*impl_->searchAction, *impl_->toolRailAction);

    impl_->toolRail = new ToolRail(*impl_->viewport, *impl_->commandActions);
    impl_->toolRail->setVisible(impl_->toolRailAction->isChecked());
    connect(impl_->toolRailAction, &QAction::toggled, this, [this](bool visible) {
        impl_->toolRail->setVisible(visible);
        // Without the rail the panel offers the sketch tools itself (U13).
        impl_->toolPanel->setToolsInRail(visible);
        QSettings().setValue(QStringLiteral("ui/toolRail"), visible);
    });
    impl_->sketchActions = std::make_unique<SketchToolActions>(*this, *impl_->toolPanel);
    impl_->toolPanel->setToolsInRail(impl_->toolRailAction->isChecked());
    impl_->toolPanel->setStateHandler([this] { impl_->sketchActions->sync(); });
    impl_->commandPalette = new CommandPalette(*this);
    connect(impl_->searchAction, &QAction::triggered, this,
            [this] { impl_->commandPalette->openSearch(); });
    impl_->contextBar = new ContextBar(*impl_->viewport, [this](std::string_view id) {
        const QString wanted = QString::fromUtf8(id.data(), static_cast<qsizetype>(id.size()));
        for (QAction* action : findChildren<QAction*>())
            if (action->property(commandIdProperty).toString() == wanted)
                return action;
        return static_cast<QAction*>(nullptr);
    });
    // Snap settings of Move/Rotate live in the context bar while that tool is active.
    QWidget& settings = impl_->contextBar->transformSettings();
    impl_->snapAction = new QAction(tr("Fangen"), this);
    impl_->snapAction->setCheckable(true);
    impl_->snapAction->setChecked(true);
    setCommandId(*impl_->snapAction, "transform.snap");
    impl_->snapAction->setToolTip(
        tr("Transformationen fangen; Strg halten, um vorübergehend frei zu bewegen"));
    impl_->linearSnapInput = makeLengthInput(&settings);
    impl_->linearSnapInput->setPrefix(tr("Linear: "));
    impl_->linearSnapInput->setValue(1.0);
    impl_->linearSnapInput->setMaximumWidth(170);
    impl_->angularSnapInput = makeAngleInput(&settings);
    impl_->angularSnapInput->setPrefix(tr("Winkel: "));
    impl_->angularSnapInput->setValue(5.0);
    impl_->angularSnapInput->setRange(0.1, 180.0);
    impl_->angularSnapInput->setMaximumWidth(170);
    settings.layout()->addWidget(impl_->linearSnapInput);
    settings.layout()->addWidget(impl_->angularSnapInput);
    createViewSwitches();
    // Controllers flip their active flag around the panel calls; read the state afterwards.
    impl_->toolPanel->setActivityHandler(
        [this] { QMetaObject::invokeMethod(this, [this] { syncShell(); }, Qt::QueuedConnection); });
}

void MainWindow::syncShell() {
    impl_->commandActions->refreshEnabled();
    syncViewSwitches();
    const ShellState shell = impl_->shell();
    // U13: the rail shows the tools of the active function.
    if (impl_->sketchActions != nullptr && impl_->toolRail != nullptr) {
        impl_->sketchActions->sync();
        if (shell.tool == ShellTool::sketchEditor && impl_->sketchActions->sketchActive()) {
            // Sketch commands exist from the first sketch on; they get their symbols then.
            if (!impl_->toolRail->showsSketch())
                applyCommandIcons(*this, impl_->themeMode);
            impl_->toolRail->showSketch(impl_->sketchActions->groups());
        }
        else
            impl_->toolRail->showSolid();
    }
    // Undo/Redo follow the document history, not the selection (U01), and wait for an open
    // tool (U05).
    impl_->undoAction->setEnabled(shell.canUndo);
    impl_->redoAction->setEnabled(shell.canRedo);
    // One card slot (U05): the active tool's panel, otherwise the selected feature.
    impl_->propertyCard->setVisible(shell.feature.has_value() && !shell.toolBlocksCommands() &&
                                    (shell.selection == ShellSelection::feature ||
                                     shell.selection == ShellSelection::sketch ||
                                     shell.selection == ShellSelection::sketchRegion ||
                                     shell.selection == ShellSelection::sketchElement));
    if (impl_->contextBar != nullptr)
        impl_->contextBar->display(contextCommands(shell));
}

} // namespace voelund::desktop
