#include <voelund/desktop/MainWindow.hpp>

#include "CommandFeedback.hpp"
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "ModelTreePanel.hpp"
#include "ShellHeader.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "Theme.hpp"
#include "ToolPanelHost.hpp"
#include "ViewCube.hpp"
#include "WorkspaceToolbar.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>
#include <voelund/viewport/CadViewport.hpp>
#include <voelund/workspaces/CommandRegistry.hpp>

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QKeySequence>
#include <QLabel>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::desktop {

MainWindow::MainWindow(application::CadSession& session, QWidget* parent)
    : QMainWindow(parent), impl_(std::make_unique<Impl>(session)) {
    impl_->geometryStatus = new QLabel(this);
    impl_->geometryStatus->setObjectName(QStringLiteral("geometryStatus"));
    statusBar()->addPermanentWidget(impl_->geometryStatus);
    createViewport();
    createModelTree();
    createPropertiesPanel();
    createToolPanel();
    createHistoryToolbar();
    createWorkspaceToolbar();
    addToolBar(impl_->repairToolbar);
    createTransformSettingsToolbar();
    impl_->documentActions =
        std::make_unique<DocumentActions>(*this, impl_->session, [this]() { refreshScene(); });
    createHeader();
    // The shell layout is fixed (ADR 0012): no floating, closing or dragging of panels.
    for (auto* dock : findChildren<QDockWidget*>())
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    for (auto* toolbar : findChildren<QToolBar*>())
        toolbar->setMovable(false);
    applyTheme(storedThemeMode());
    connectInteractions();
    syncProperties();
    syncTreeStatus();
    syncSketchPresentation();
    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    event->setAccepted(impl_->documentActions->confirmClose());
}

void MainWindow::createViewport() {
    impl_->viewport = new viewport::CadViewport(this);
    const std::vector<viewport::SceneBody> bodies = makeSceneBodies(impl_->session);
    impl_->viewport->setBodies(bodies);
    impl_->viewCube = new ViewCube(*impl_->viewport);
    setCentralWidget(impl_->viewport);
}

void MainWindow::createModelTree() {
    impl_->modelTree = new ModelTreePanel(this);
    auto* dock = new QDockWidget(tr("Model"), this);
    dock->setObjectName(QStringLiteral("modelDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setWidget(impl_->modelTree);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    syncModelTree();
}

void MainWindow::createWorkspaceToolbar() {
    modules::solid::SolidCommandServices services;
    services.addBox = [this]() {
        if (!runCommand([this] { return impl_->session.createBoxFeature(); },
                        [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Box feature created"), 2000);
    };
    services.addCylinder = [this]() {
        if (!runCommand(
                [this] {
                    return impl_->session.createCylinderFeature(document::CylinderResultMode::join);
                },
                [this](const QString& message) { statusBar()->showMessage(message); }))
            return;
        refreshScene();
        statusBar()->showMessage(tr("Cylinder feature created"), 2000);
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
    impl_->workspaceToolbar =
        std::make_unique<WorkspaceToolbar>(*this, impl_->commandRegistry.module("solid"));
    impl_->addBoxAction = impl_->workspaceToolbar->action("solid.create.box");
    impl_->addCylinderAction = impl_->workspaceToolbar->action("solid.create.cylinder");
    impl_->addCylinderAction->setObjectName(QStringLiteral("addCylinderAction"));
    impl_->sketchEditorAction = impl_->workspaceToolbar->action("solid.create.sketch");
    impl_->sketchEditorAction->setObjectName(QStringLiteral("sketchEditorAction"));
    impl_->addBoxAction->setObjectName(QStringLiteral("addBoxAction"));
    impl_->transformAction = impl_->workspaceToolbar->action("solid.modify.transform");
    impl_->transformAction->setObjectName(QStringLiteral("transformAction"));
}

void MainWindow::createToolPanel() {
    impl_->toolPanel = new ToolPanelHost(this);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolPanel);
    tabifyDockWidget(impl_->propertyDock, impl_->toolPanel);
    impl_->propertyDock->raise();
    impl_->sketchEditorTool = std::make_unique<SketchEditorToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->sketchExtrusionTool = std::make_unique<SketchExtrusionToolController>(
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
}

void MainWindow::createPropertiesPanel() {
    impl_->propertyPanel = new FeaturePropertiesPanel(this);
    auto* dock = new QDockWidget(tr("Properties"), this);
    dock->setObjectName(QStringLiteral("featurePropertiesDock"));
    dock->setWidget(impl_->propertyPanel);
    dock->setMinimumWidth(270);
    dock->setMaximumWidth(360);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    impl_->propertyDock = dock;
}

void MainWindow::createHistoryToolbar() {
    impl_->undoAction = new QAction(tr("Undo"), this);
    impl_->undoAction->setShortcut(QKeySequence::Undo);
    impl_->redoAction = new QAction(tr("Redo"), this);
    impl_->redoAction->setShortcuts(QKeySequence::Redo);
    setCommandId(*impl_->undoAction, "history.undo");
    setCommandId(*impl_->redoAction, "history.redo");
    // Undo/Redo sit in the shell header; repair stays here until U10 moves it to the feature.
    auto* toolbar = new QToolBar(tr("Repair"), this);
    toolbar->setObjectName(QStringLiteral("repairToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    impl_->repairToolbar = toolbar;
    impl_->repairAction = toolbar->addAction(tr("Repair references…"));
    impl_->repairAction->setObjectName(QStringLiteral("repairReferencesAction"));
    setCommandId(*impl_->repairAction, "repair.references");
    connect(impl_->repairAction, &QAction::triggered, this, [this] { repairReferences(); });
    impl_->repairAllAction = toolbar->addAction(tr("Repair all…"));
    impl_->repairAllAction->setObjectName(QStringLiteral("repairAllReferencesAction"));
    setCommandId(*impl_->repairAllAction, "repair.all");
    connect(impl_->repairAllAction, &QAction::triggered, this, [this] { repairAllReferences(); });
}

void MainWindow::createHeader() {
    impl_->themeAction = new QAction(tr("Switch light/dark theme"), this);
    setCommandId(*impl_->themeAction, "view.theme");
    connect(impl_->themeAction, &QAction::triggered, this, [this] {
        const ThemeMode next =
            impl_->themeMode == ThemeMode::dark ? ThemeMode::light : ThemeMode::dark;
        storeThemeMode(next);
        applyTheme(next);
    });
    impl_->header = new ShellHeader(this);
    impl_->header->setFileMenu(impl_->documentActions->menu());
    impl_->header->setActions(*impl_->undoAction, *impl_->redoAction, *impl_->themeAction);
    setMenuWidget(impl_->header);
    impl_->documentActions->setShellChangedHandler(
        [this] { impl_->header->display(impl_->shell()); });
}

void MainWindow::applyTheme(ThemeMode mode) {
    impl_->themeMode = mode;
    setStyleSheet(themeStyleSheet(themeTokens(mode)));
    impl_->viewport->setTheme(viewportTheme(mode));
    impl_->viewCube->setTokens(themeTokens(mode));
    impl_->header->setTheme(mode);
}

void MainWindow::createTransformSettingsToolbar() {
    // Settings start a second row so wider styles (e.g. Breeze) never move them into a
    // toolbar overflow menu.
    addToolBarBreak();
    impl_->snapAction = new QAction(tr("Snap"), this);
    impl_->snapAction->setCheckable(true);
    impl_->snapAction->setChecked(true);
    setCommandId(*impl_->snapAction, "transform.snap");
    impl_->snapAction->setToolTip(tr("Snap transformations; hold Ctrl to bypass temporarily"));
    auto* toolbar = addToolBar(tr("Transform"));
    toolbar->setObjectName(QStringLiteral("transformToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(impl_->snapAction);
    impl_->linearSnapInput = makeLengthInput(toolbar);
    impl_->linearSnapInput->setPrefix(tr("Linear: "));
    impl_->linearSnapInput->setValue(1.0);
    impl_->linearSnapInput->setMaximumWidth(145);
    impl_->angularSnapInput = makeAngleInput(toolbar);
    impl_->angularSnapInput->setPrefix(tr("Angular: "));
    impl_->angularSnapInput->setValue(5.0);
    impl_->angularSnapInput->setRange(0.1, 180.0);
    impl_->angularSnapInput->setMaximumWidth(145);
    toolbar->addWidget(impl_->linearSnapInput);
    toolbar->addWidget(impl_->angularSnapInput);
}

void MainWindow::connectInteractions() {
    impl_->propertyPanel->setBoxChangeHandler([this]() { applyBoxParameters(); });
    impl_->propertyPanel->setCylinderChangeHandler([this]() { applyCylinderParameters(); });
    impl_->propertyPanel->setExtrusionChangeHandler([this]() { applyExtrusionParameters(); });
    impl_->modelTree->setFeatureSelectionHandler([this](std::uint64_t id) { selectFeature(id); });
    impl_->viewport->setBodySelectionHandler([this](std::uint64_t id) { selectFeature(id); });
    impl_->viewport->setPlanarFaceSelectionHandler(
        [this](const viewport::PlanarFaceSelection& selection) {
            impl_->selectedDatum.reset();
            if (impl_->session.sceneStatus().isCurrent()) {
                impl_->selectedPlanarFace = selection;
                statusBar()->showMessage(tr("Planar face selected"), 2000);
            } else {
                impl_->selectedPlanarFace.reset();
                statusBar()->showMessage(tr("Repair the model before selecting geometry"), 3000);
            }
            impl_->workspaceToolbar->refreshEnabled();
        });
    impl_->viewport->setBasePlaneSelectionHandler([this](viewport::BasePlane plane) {
        impl_->selectedPlanarFace.reset();
        impl_->selectedDatum = static_cast<document::DatumPlane>(plane);
        static const char* const names[]{"XY", "XZ", "YZ"};
        statusBar()->showMessage(
            tr("%1 plane selected").arg(QString::fromLatin1(names[static_cast<int>(plane) - 1])),
            2000);
        impl_->workspaceToolbar->refreshEnabled();
    });
    impl_->viewport->setSketchEditorPreviewHandler(
        [this](const std::optional<viewport::SketchElementIntent>& intent) {
            impl_->sketchEditorTool->updatePreview(intent);
        });
    impl_->viewport->setSketchEditorElementHandler(
        [this](const viewport::SketchElementIntent& intent) {
            impl_->sketchEditorTool->confirmElement(intent);
        });
    impl_->viewport->setSketchEditorTranslationHandler([this](double deltaX, double deltaY) {
        impl_->sketchEditorTool->translateFromViewport(deltaX, deltaY);
    });
    impl_->viewport->setSketchConstraintEditHandler(
        [this](std::uint64_t id) { impl_->sketchEditorTool->editConstraint(id); });
    impl_->viewport->setSketchElementSelectionHandler(
        [this](const viewport::SketchSelectionIntent& intent) {
            if (impl_->sketchEditorTool->isActive()) {
                static_cast<void>(impl_->sketchEditorTool->selectElement(intent));
                return;
            }
            const std::uint64_t id = intent.elementId;
            if (impl_->session.selectSketchProfileByElement(document::SketchElementId{id},
                                                            intent.additive)) {
                const auto selectedElements = impl_->session.selectedSketchProfileElements();
                std::vector<std::uint64_t> selectedIds;
                selectedIds.reserve(selectedElements.size());
                for (document::SketchElementId selected : selectedElements)
                    selectedIds.push_back(selected.value());
                impl_->viewport->setSelectedSketchElements(selectedIds);
                syncProperties();
                impl_->workspaceToolbar->refreshEnabled();
                statusBar()->showMessage(tr("Sketch profile selected"), 2000);
                return;
            }
            if (!intent.additive &&
                impl_->session.selectSketchElement(document::SketchElementId{id})) {
                impl_->viewport->setSelectedSketchElement(id);
                syncProperties();
                statusBar()->showMessage(tr("Sketch element selected"), 2000);
            }
        });
    impl_->viewport->setBodyMoveHandler(
        [this](std::uint64_t id, double deltaX, double deltaY, double deltaZ) {
            if (!selectTransformFeatureForBody(impl_->session, document::FeatureId{id})) {
                return;
            }
            kernel::RigidTransform placement = impl_->session.selectedFeaturePlacement();
            placement.translationX += deltaX;
            placement.translationY += deltaY;
            placement.translationZ += deltaZ;
            applyParameterCommand([this, placement](application::ErrorDisposition disposition) {
                return impl_->session.editSelectedFeaturePlacement(placement, disposition);
            });
        });
    impl_->viewport->setBodyMovePreviewHandler(
        [this](std::uint64_t id, double deltaX, double deltaY, double deltaZ) {
            previewBodyMove(id, deltaX, deltaY, deltaZ);
        });
    const auto updateSnapping = [this]() {
        impl_->viewport->setTransformSnapping(impl_->snapAction->isChecked(),
                                              impl_->linearSnapInput->value(),
                                              impl_->angularSnapInput->value());
    };
    connect(impl_->snapAction, &QAction::toggled, this,
            [updateSnapping](bool) { updateSnapping(); });
    connect(impl_->linearSnapInput, &QDoubleSpinBox::valueChanged, this,
            [updateSnapping](double) { updateSnapping(); });
    connect(impl_->angularSnapInput, &QDoubleSpinBox::valueChanged, this,
            [updateSnapping](double) { updateSnapping(); });
    updateSnapping();
    impl_->viewport->setBodyRotationHandler(
        [this](std::uint64_t id, int axisIndex, double angleDegrees) {
            if (!selectTransformFeatureForBody(impl_->session, document::FeatureId{id})) {
                return;
            }
            kernel::RigidTransform placement = impl_->session.selectedFeaturePlacement();
            kernel::Radians* rotations[] = {&placement.rotationX, &placement.rotationY,
                                            &placement.rotationZ};
            if (axisIndex < 0 || axisIndex > 2) {
                return;
            }
            *rotations[axisIndex] =
                kernel::Radians(rotations[axisIndex]->value() +
                                kernel::toRadians(kernel::Degrees(angleDegrees)).value());
            applyParameterCommand([this, placement](application::ErrorDisposition disposition) {
                return impl_->session.editSelectedFeaturePlacement(placement, disposition);
            });
        });
    impl_->viewport->setBodyRotationPreviewHandler(
        [this](std::uint64_t id, int axisIndex, double angleDegrees) {
            previewBodyRotation(id, axisIndex, angleDegrees);
        });
    connect(impl_->undoAction, &QAction::triggered, this, [this]() { applyHistoryCommand(false); });
    connect(impl_->redoAction, &QAction::triggered, this, [this]() { applyHistoryCommand(true); });
}

void MainWindow::syncModelTree() {
    impl_->modelTree->showFeatures(impl_->session.features(), impl_->session.selectedFeature());
}

void MainWindow::syncProperties() {
    impl_->pendingParameter = {};
    impl_->pendingStamp.reset();
    impl_->propertyPanel->setErrorCommitHandler({});
    const ShellState shell = impl_->shell();
    const std::optional<document::FeatureId> selected = shell.feature;
    impl_->repairAllAction->setEnabled(!impl_->session.features().empty());
    impl_->repairAction->setEnabled(shell.canEditSketch());
    impl_->workspaceToolbar->refreshEnabled();
    // Undo/Redo follow the document history, not the selection (U01).
    impl_->undoAction->setEnabled(shell.canUndo);
    impl_->redoAction->setEnabled(shell.canRedo);
    if (!selected.has_value()) {
        impl_->propertyPanel->showNone();
        syncTransformTool();
        impl_->documentActions->syncWindowTitle();
        return;
    }
    const document::FeatureDescriptor& feature = impl_->session.selectedFeatureDescriptor();
    if (feature.kind == document::FeatureKind::box) {
        impl_->propertyPanel->showBox(impl_->session.selectedBoxParameters());
    } else if (feature.kind == document::FeatureKind::cylinder) {
        const document::CylinderParameters& parameters =
            impl_->session.selectedCylinderParameters();
        impl_->propertyPanel->showCylinder(parameters,
                                           featureErrorText(impl_->session, feature.id));
    } else if (feature.kind == document::FeatureKind::extrusion) {
        impl_->propertyPanel->showExtrusion(impl_->session.selectedExtrusionParameters(),
                                            featureErrorText(impl_->session, feature.id));
    } else {
        const QString title = feature.kind == document::FeatureKind::sketch
                                  ? sketchSelectionSummary(impl_->session)
                                  : tr("Feature");
        impl_->propertyPanel->showFeatureInfo(title, featureErrorText(impl_->session, feature.id));
    }
    syncTransformTool();
    impl_->documentActions->syncWindowTitle();
}

void MainWindow::syncTreeStatus() {
    syncGeometryStatus();
    impl_->modelTree->showStatuses(impl_->session.featureStates());
}

void MainWindow::syncSketchPresentation() {
    const std::vector<viewport::SceneSketchElement> elements =
        makeSceneSketchElements(impl_->session);
    impl_->viewport->setSketchElements(elements);
    const auto profileElements = impl_->session.selectedSketchProfileElements();
    std::vector<std::uint64_t> selectedIds;
    selectedIds.reserve(profileElements.size());
    for (document::SketchElementId selected : profileElements)
        selectedIds.push_back(selected.value());
    if (selectedIds.empty()) {
        const auto selected = impl_->session.selectedSketchElement();
        if (selected.has_value())
            selectedIds.push_back(selected->value());
    }
    impl_->viewport->setSelectedSketchElements(selectedIds);
}

void MainWindow::syncTransformTool() {
    const std::optional<document::FeatureId> selected = impl_->session.selectedFeature();
    const bool canTransform = selected.has_value() && impl_->session.canTransformFeature(*selected);
    const std::optional<document::FeatureId> body =
        canTransform ? impl_->session.transformBodyForFeature(*selected) : std::nullopt;
    impl_->viewport->setTransformTarget(
        body.has_value() ? std::optional<std::uint64_t>{body->value()} : std::nullopt);
    if (!canTransform && impl_->transformAction->isChecked()) {
        const QSignalBlocker blocker(impl_->transformAction);
        impl_->transformAction->setChecked(false);
        impl_->viewport->setTransformToolEnabled(false);
    }
    impl_->transformAction->setEnabled(canTransform);
}

void MainWindow::refreshScene() {
    if (impl_->sketchEditorTool->isActive()) {
        impl_->sketchEditorTool->cancel();
    }
    impl_->selectedPlanarFace.reset();
    impl_->selectedDatum.reset();
    impl_->viewport->cancelSketch();
    impl_->toolPanel->end();
    const std::vector<viewport::SceneBody> bodies = makeSceneBodies(impl_->session);
    impl_->viewport->setBodies(bodies);
    syncModelTree();
    syncProperties();
    syncTreeStatus();
    syncSketchPresentation();
}

void MainWindow::previewBodyMove(std::uint64_t id, double deltaX, double deltaY, double deltaZ) {
    const document::FeatureId featureId{id};
    if (!selectTransformFeatureForBody(impl_->session, featureId)) {
        return;
    }
    impl_->propertyPanel->previewMove(impl_->session.selectedFeaturePlacement(), deltaX, deltaY,
                                      deltaZ);
}

void MainWindow::previewBodyRotation(std::uint64_t id, int axisIndex, double angleDegrees) {
    const document::FeatureId featureId{id};
    if (!selectTransformFeatureForBody(impl_->session, featureId) || axisIndex < 0 ||
        axisIndex > 2) {
        return;
    }
    impl_->propertyPanel->previewRotation(impl_->session.selectedFeaturePlacement(), axisIndex,
                                          angleDegrees);
}

void MainWindow::selectFeature(std::uint64_t rawId) {
    const document::FeatureId id{rawId};
    if (!impl_->session.selectFeature(id)) {
        return;
    }
    impl_->modelTree->selectFeature(id);
    syncProperties();
    syncSketchPresentation();
}

} // namespace voelund::desktop
