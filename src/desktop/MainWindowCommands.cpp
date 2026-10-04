#include <voelund/desktop/MainWindow.hpp>

#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"

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
        if (!runCommand([this] { return impl_->session.createBoxFeature(); },
                        [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Quader angelegt"), 2000);
    };
    services.addCylinder = [this]() {
        if (!runCommand(
                [this] {
                    return impl_->session.createCylinderFeature(document::CylinderResultMode::join);
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
    services.canAddCylinder = [this]() { return impl_->session.canAddCylinder(); };
    // Enables "Sketch": a picked planar face or a picked base plane (V01b).
    services.canEditSketch = [this]() { return impl_->shell().canSketch(); };
    services.canEditExistingSketch = [this]() { return impl_->shell().canEditSketch(); };
    services.canExtrudeProfiles = [this]() { return impl_->shell().canExtrude(); };
    services.canTransform = [this]() {
        const auto selected = impl_->session.selectedFeature();
        return selected.has_value() && impl_->session.canTransformFeature(*selected);
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
        QSettings().setValue(QStringLiteral("ui/toolRail"), visible);
    });
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
    // Controllers flip their active flag around the panel calls; read the state afterwards.
    impl_->toolPanel->setActivityHandler(
        [this] { QMetaObject::invokeMethod(this, [this] { syncShell(); }, Qt::QueuedConnection); });
}

void MainWindow::syncShell() {
    impl_->commandActions->refreshEnabled();
    const ShellState shell = impl_->shell();
    if (impl_->contextBar != nullptr)
        impl_->contextBar->display(contextCommands(shell));
}

} // namespace voelund::desktop
