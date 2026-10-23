// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "SectionController.hpp"

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
                    // M01 (user decision 2026-10-10): joined to the selected body; without
                    // one a new body. Never the last feature of the document.
                    const auto body = impl_->session.selectedBodyProducer();
                    return runAction(impl_->session,
                                     application::CreateCylinderAction{
                                         body, body ? document::CylinderResultMode::join
                                                    : document::CylinderResultMode::separate});
                },
                [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Zylinder angelegt"), 2000);
    };
    services.primitive = [this](int shape) { // M27
        const auto kind = static_cast<kernel::PrimitiveShape>(shape);
        if (!runCommand(
                [this, kind] {
                    const auto body = impl_->session.selectedBodyProducer();
                    return runAction(impl_->session,
                                     application::CreatePrimitiveAction{
                                         kind, body,
                                         body ? document::CylinderResultMode::join
                                              : document::CylinderResultMode::separate});
                },
                [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(
            tr("%1 angelegt").arg(tr(application::CadSession::primitiveShapeName(kind))), 2000);
    };
    services.canPrimitive = [this]() {
        return impl_->session.canAddCylinder() && !impl_->shell().toolBlocksCommands();
    };
    services.sketchEditor = [this]() {
        // M19: a sketch on the selected construction plane.
        if (const auto selected = impl_->session.selectedFeature();
            selected && !impl_->selectedDatum && !impl_->selectedPlanarFace &&
            impl_->session.selectedFeatureDescriptor().kind == document::FeatureKind::datumPlane) {
            if (impl_->sketchExtrusionTool->isActive())
                impl_->sketchExtrusionTool->cancel();
            impl_->sketchEditorTool->beginOnConstructionPlane(*selected);
            return;
        }
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
        // M04: an extrusion reopens its panel; M06: a fillet its panel; a sketch its editor.
        const auto selected = impl_->session.selectedFeature();
        const auto kind = selected ? std::optional{impl_->session.selectedFeatureDescriptor().kind}
                                   : std::nullopt;
        if (kind == document::FeatureKind::extrusion) {
            impl_->sketchExtrusionTool->beginEdit(*selected);
            syncSketchPresentation();
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::fillet || kind == document::FeatureKind::chamfer) {
            impl_->filletTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::hole) {
            impl_->holeTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::combine) {
            impl_->combineTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::shell) {
            impl_->shellTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::draft) { // M25
            impl_->draftTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::mirror) { // M09
            impl_->mirrorTool->beginEdit(*selected);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::move) { // M11
            impl_->moveTool->beginEdit(*selected);
            syncShell();
            return;
        }
        // M10: a pattern, also through one of its copies.
        if (kind == document::FeatureKind::pattern || kind == document::FeatureKind::patternCopy) {
            impl_->patternTool->beginEdit(kind == document::FeatureKind::pattern
                                              ? *selected
                                              : impl_->session.identifiedSnapshot()
                                                    .snapshot()
                                                    .patternCopyParameters(*selected)
                                                    .pattern);
            syncShell();
            return;
        }
        if (kind == document::FeatureKind::datumPlane || kind == document::FeatureKind::datumAxis) {
            impl_->datumTool->beginEdit(*selected);
            syncShell();
            return;
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
    services.canEditExistingSketch = [this]() { return impl_->shell().canEditFeature(); };
    services.canExtrudeProfiles = [this]() { return impl_->shell().canExtrude(); };
    services.revolve = [this]() { // M20
        if (impl_->sketchEditorTool->isActive())
            impl_->sketchEditorTool->cancel();
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->sketchExtrusionTool->beginSelectedProfiles(true);
    };
    services.helix = [this]() { // M26
        if (impl_->sketchEditorTool->isActive())
            impl_->sketchEditorTool->cancel();
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->sketchExtrusionTool->beginSelectedProfiles(true, true);
    };
    services.loft = [this]() { // M24
        if (impl_->sketchEditorTool->isActive())
            impl_->sketchEditorTool->cancel();
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->sketchExtrusionTool->beginSelectedLoft();
    };
    services.sweep = [this]() { // M23
        if (impl_->sketchEditorTool->isActive())
            impl_->sketchEditorTool->cancel();
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->sketchExtrusionTool->beginSelectedSweep();
    };
    // M06: rounds edges of the selected body's current result.
    const auto treatEdges = [this](EdgeTreatment treatment) {
        // Edges picked before (user request 2026-10-13) are taken over.
        auto edges = impl_->selectedEdges;
        auto producer = edges.empty() ? impl_->session.selectedBodyProducer() : impl_->edgeSupport;
        if (!producer && !impl_->session.validBodies().empty()) // nothing selected: the newest
            producer = impl_->session.validBodies().back().sourceFeature; // until an edge is hit
        if (!producer)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        clearEdgeSelection();
        impl_->filletTool->begin(*producer, treatment, std::move(edges));
        syncShell();
    };
    services.fillet = [treatEdges]() { treatEdges(EdgeTreatment::fillet); };
    services.chamfer = [treatEdges]() { treatEdges(EdgeTreatment::chamfer); }; // M07
    // T04c: push/pull of the picked planar face. W04 (Q, as in Fusion's Press Pull): picked
    // edges are rounded and a selected sketch region is extruded instead.
    services.pushPull = [this, treatEdges, extrude = services.extrudeProfiles]() {
        if (!impl_->selectedEdges.empty()) {
            treatEdges(EdgeTreatment::fillet);
            return;
        }
        if (impl_->shell().canExtrude()) {
            extrude();
            return;
        }
        if (!impl_->selectedPlanarFace)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->pushPullTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    services.canPushPull = [this]() {
        const auto shell = impl_->shell();
        return !shell.toolBlocksCommands() &&
               (!impl_->selectedEdges.empty() || shell.canExtrude() ||
                (impl_->selectedPlanarFace.has_value() && shell.canSketch()));
    };
    // M08: a hole at the clicked point of the picked planar face.
    services.hole = [this]() {
        if (!impl_->selectedPlanarFace)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->holeTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    // M21: the selected body takes the result; other bodies are clicked as tools.
    services.combine = [this]() {
        const auto producer = impl_->session.selectedBodyProducer();
        if (!producer)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        clearEdgeSelection();
        impl_->combineTool->begin(*producer);
        syncShell();
    };
    // M09: mirrors the selected body; a picked planar face is its body and the plane.
    services.mirror = [this]() {
        const auto face = impl_->selectedPlanarFace;
        const auto producer = face ? std::optional{document::FeatureId{face->bodyId}}
                                   : impl_->session.selectedBodyProducer();
        if (!producer)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        clearEdgeSelection();
        impl_->mirrorTool->begin(*producer, face);
        syncShell();
    };
    // M12: measuring changes nothing; it needs a current model with a body.
    services.measure = [this]() {
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        clearEdgeSelection();
        impl_->measureTool->begin();
        syncShell();
    };
    services.canMeasure = [this]() {
        return impl_->session.sceneStatus().isCurrent() && !impl_->session.validBodies().empty() &&
               !impl_->shell().toolBlocksCommands();
    };
    services.canPattern = [this]() {
        return (impl_->selectedPlanarFace.has_value() && impl_->shell().canSketch()) ||
               ((impl_->session.selectedBodyProducer().has_value() || impl_->edgeSupport) &&
                impl_->session.sceneStatus().isCurrent() && !impl_->shell().toolBlocksCommands());
    };
    services.canMirror = [this]() {
        return (impl_->selectedPlanarFace.has_value() && impl_->shell().canSketch()) ||
               (impl_->session.selectedBodyProducer().has_value() &&
                impl_->session.sceneStatus().isCurrent() && !impl_->shell().toolBlocksCommands());
    };
    // M10: copies of the selected body; one edge picked before is the direction or axis.
    const auto pattern = [this](document::PatternKind kind) {
        const auto face = impl_->selectedPlanarFace;
        const auto producer = face ? std::optional{document::FeatureId{face->bodyId}}
                              : impl_->edgeSupport ? impl_->edgeSupport
                                                   : impl_->session.selectedBodyProducer();
        if (!producer)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        const bool oneEdge = impl_->selectedEdges.size() == 1 && impl_->edgeSupport;
        const auto edge = oneEdge ? std::optional{impl_->selectedEdges.front()} : std::nullopt;
        const auto edgeSupport = oneEdge ? impl_->edgeSupport : std::nullopt;
        clearEdgeSelection();
        impl_->patternTool->begin(*producer, kind, edgeSupport, edge);
        syncShell();
    };
    services.patternLinear = [pattern]() { pattern(document::PatternKind::linear); };
    services.patternCircular = [pattern]() { pattern(document::PatternKind::circular); };
    // M19: a construction plane on the picked base plane or planar face (XY without one), and
    // a construction axis: on the one picked edge at once, else a base axis in the panel.
    services.datumPlane = [this]() {
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        if (impl_->selectedPlanarFace) {
            const document::FeatureId support{impl_->selectedPlanarFace->bodyId};
            std::optional<document::SubshapeOrigin> face;
            try {
                face =
                    impl_->session.faceReference(support, impl_->selectedPlanarFace->selectedFace)
                        .origin;
            } catch (const std::exception&) {
                statusBar()->showMessage(tr("Diese Fläche hat keinen festen Namen."), 3000);
                return;
            }
            impl_->datumTool->beginPlane(std::nullopt, support, face);
        } else {
            impl_->datumTool->beginPlane(impl_->selectedDatum, std::nullopt, std::nullopt);
        }
        syncShell();
    };
    services.datumAxis = [this]() {
        if (impl_->selectedEdges.size() == 1 && impl_->edgeSupport) {
            const auto edge = impl_->selectedEdges.front();
            const auto support = *impl_->edgeSupport;
            if (!runCommand(
                    [&] {
                        return runAction(impl_->session, application::CreateDatumAxisAction{
                                                             std::nullopt, support, edge});
                    },
                    [this](const QString& message) { statusBar()->showMessage(message); }))
                return;
            clearEdgeSelection();
            refreshScene();
            statusBar()->showMessage(tr("Bezugsachse angelegt"), 2000);
            return;
        }
        impl_->datumTool->beginAxis();
        syncShell();
    };
    services.canDatum = [this]() { return !impl_->shell().toolBlocksCommands(); };
    // M22: walls with the picked planar face opened.
    services.shell = [this]() {
        if (!impl_->selectedPlanarFace)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->shellTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    // M25: a draft about the picked planar face as the neutral face.
    services.draft = [this]() {
        if (!impl_->selectedPlanarFace)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        impl_->draftTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    services.canHole = [this]() {
        return impl_->selectedPlanarFace.has_value() && impl_->shell().canSketch();
    };
    services.canCombine = [this]() { return canCombineBody(); };
    services.canFillet = [this]() { return canTreatEdges(); };
    services.canTransform = [this]() {
        const auto selected = impl_->session.selectedFeature();
        return selected.has_value() && impl_->session.canTransformFeature(*selected) &&
               !impl_->shell().toolBlocksCommands();
    };
    impl_->commandRegistry.registerModule(
        std::make_unique<modules::solid::SolidWorkspaceModule>(std::move(services)));
    impl_->commandActions =
        std::make_unique<CommandActions>(*this, impl_->commandRegistry.module("solid"));
    // Names for the smoke test and older desktop tests.
    impl_->commandActions->action("solid.create.box")
        ->setObjectName(QStringLiteral("addBoxAction"));
    impl_->commandActions->action("solid.create.cylinder")
        ->setObjectName(QStringLiteral("addCylinderAction"));
    impl_->commandActions->action("solid.create.sketch")
        ->setObjectName(QStringLiteral("sketchEditorAction"));
    impl_->transformAction = impl_->commandActions->action("solid.modify.transform");
    impl_->transformAction->setObjectName(QStringLiteral("transformAction"));
}

// M21: the selected body takes the result.
bool MainWindow::canCombineBody() const {
    const auto selected = impl_->session.selectedFeature();
    return selected &&
           impl_->session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch &&
           impl_->session.selectedBodyProducer().has_value() &&
           impl_->session.sceneStatus().isCurrent() && !impl_->shell().toolBlocksCommands();
}

// User report 2026-10-23 (as in Fusion): Verrunden and Fase stay offered while there is a body;
// with nothing selected the panel opens and the first edge clicked picks the body.
bool MainWindow::canTreatEdges() const {
    return !impl_->session.validBodies().empty() && impl_->session.sceneStatus().isCurrent() &&
           !impl_->shell().toolBlocksCommands();
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
    impl_->toolPanel->setActivityHandler([this] {
        QMetaObject::invokeMethod(
            this,
            [this] {
                // M04: an extrusion panel shows its sketch while open; afterwards tree,
                // timeline, properties and view follow the selection again (an edited
                // extrusion).
                const auto selected = impl_->session.selectedFeature();
                if (!impl_->shell().toolBlocksCommands() && selected)
                    selectFeature(selected->value());
                else if (!impl_->sketchEditorTool->isActive())
                    syncSketchPresentation();
                syncShell();
            },
            Qt::QueuedConnection);
    });
}

namespace {
void syncSection(SectionController* section) {
    if (section != nullptr)
        section->sync();
}
} // namespace

void MainWindow::syncShell() {
    impl_->commandActions->refreshEnabled();
    syncParameters(); // PAR02: the table stays fixed while a tool is open
    syncBodyCommands();
    syncViewSwitches();
    syncSection(impl_->section); // N05: new construction planes, a selected face
    const ShellState shell = impl_->shell();
    // U13: the rail shows the tools of the active function.
    if (impl_->sketchActions != nullptr && impl_->toolRail != nullptr) {
        impl_->sketchActions->sync();
        if (shell.tool == ShellTool::sketchEditor && impl_->sketchActions->sketchActive()) {
            // Sketch commands exist from the first sketch on; they get their symbols then.
            if (!impl_->toolRail->showsSketch())
                applyCommandIcons(*this, impl_->themeMode);
            impl_->toolRail->showSketch(impl_->sketchActions->sections());
        } else
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
