#include <voelund/desktop/MainWindow.hpp>

#include "CommandFeedback.hpp"
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "ModelTreePanel.hpp"
#include "ShellHeader.hpp"
#include "ShellIcons.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "Theme.hpp"
#include "ToolPanelHost.hpp"
#include "ViewCube.hpp"

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
#include <QVBoxLayout>
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
    createHistoryActions();
    createCommands();
    impl_->documentActions =
        std::make_unique<DocumentActions>(*this, impl_->session, [this]() { refreshScene(); });
    createHeader();
    createCommandSurfaces();
    createTimeline();
    // The shell layout is fixed (ADR 0012): no floating, closing or dragging of panels.
    for (auto* dock : findChildren<QDockWidget*>())
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    applyTheme(storedThemeMode());
    connectInteractions();
    syncVisibility(); // Origin, planes and grid follow the model tree from the start (U06).
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
    impl_->viewport->setBodies(makeSceneBodies(impl_->session));
    impl_->viewCube = new ViewCube(*impl_->viewport);
    setCentralWidget(impl_->viewport);
}

void MainWindow::createModelTree() {
    impl_->modelTree = new ModelTreePanel(this);
    auto* dock = new QDockWidget(tr("Modell"), this);
    dock->setObjectName(QStringLiteral("modelDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setWidget(impl_->modelTree);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    syncModelTree();
}

void MainWindow::createToolPanel() {
    impl_->toolPanel = new ToolPanelHost(impl_->viewport);
    impl_->sketchEditorTool = std::make_unique<SketchEditorToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->sketchExtrusionTool = std::make_unique<SketchExtrusionToolController>(
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
}

void MainWindow::createPropertiesPanel() {
    // U05: properties of the selected feature float in the same slot as the tool panel.
    impl_->propertyCard =
        new FloatingCard(*impl_->viewport, QStringLiteral("featurePropertiesCard"));
    impl_->propertyCard->setTitle(tr("Eigenschaften"));
    impl_->propertyPanel = new FeaturePropertiesPanel(impl_->propertyCard);
    impl_->propertyCard->body().addWidget(impl_->propertyPanel);
}

void MainWindow::createHistoryActions() {
    impl_->undoAction = new QAction(tr("Rückgängig"), this);
    impl_->undoAction->setShortcut(QKeySequence::Undo);
    impl_->redoAction = new QAction(tr("Wiederholen"), this);
    impl_->redoAction->setShortcuts(QKeySequence::Redo);
    setCommandId(*impl_->undoAction, "history.undo");
    setCommandId(*impl_->redoAction, "history.redo");
    // Undo/Redo sit in the shell header. Repair is offered by the context bar and the command
    // search until U10 moves it to the feature.
    impl_->repairAction = new QAction(tr("Referenzen reparieren…"), this);
    impl_->repairAction->setObjectName(QStringLiteral("repairReferencesAction"));
    setCommandId(*impl_->repairAction, "repair.references");
    connect(impl_->repairAction, &QAction::triggered, this, [this] { repairReferences(); });
    impl_->repairAllAction = new QAction(tr("Alle reparieren…"), this);
    impl_->repairAllAction->setObjectName(QStringLiteral("repairAllReferencesAction"));
    setCommandId(*impl_->repairAllAction, "repair.all");
    connect(impl_->repairAllAction, &QAction::triggered, this, [this] { repairAllReferences(); });
}

void MainWindow::createHeader() {
    impl_->themeAction = new QAction(tr("Helle/dunkle Darstellung umschalten"), this);
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
    applyCommandIcons(*this, mode);
    if (impl_->timeline != nullptr)
        impl_->timeline->setTheme(mode);
    const ThemeTokens& tokens = themeTokens(mode);
    impl_->modelTree->setEyeIcons(shellIcon(ShellIcon::eye, tokens.muted, tokens.accent),
                                  shellIcon(ShellIcon::eyeOff, tokens.faint, tokens.accent));
}

void MainWindow::connectInteractions() {
    impl_->propertyPanel->setBoxChangeHandler([this]() { applyBoxParameters(); });
    impl_->propertyPanel->setCylinderChangeHandler([this]() { applyCylinderParameters(); });
    impl_->propertyPanel->setExtrusionChangeHandler([this]() { applyExtrusionParameters(); });
    impl_->modelTree->setHandlers({[this](std::uint64_t id) { selectFeature(id); },
                                   [this](int plane) { selectBasePlane(plane); },
                                   [this](ModelTreePanel::Kind kind, std::uint64_t id) {
                                       toggleVisibility(static_cast<int>(kind), id);
                                   }});
    impl_->viewport->setBodySelectionHandler([this](std::uint64_t id) { selectFeature(id); });
    impl_->viewport->setPlanarFaceSelectionHandler(
        [this](const viewport::PlanarFaceSelection& selection) {
            impl_->selectedDatum.reset();
            if (impl_->session.sceneStatus().isCurrent()) {
                impl_->selectedPlanarFace = selection;
                statusBar()->showMessage(tr("Ebene Fläche gewählt"), 2000);
            } else {
                impl_->selectedPlanarFace.reset();
                statusBar()->showMessage(tr("Repariere das Modell, bevor du Geometrie wählst"),
                                         3000);
            }
            syncShell();
        });
    impl_->viewport->setBasePlaneSelectionHandler(
        [this](viewport::BasePlane plane) { selectBasePlane(static_cast<int>(plane)); });
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
                syncShell();
                statusBar()->showMessage(tr("Skizzenbereich gewählt"), 2000);
                return;
            }
            if (!intent.additive &&
                impl_->session.selectSketchElement(document::SketchElementId{id})) {
                impl_->viewport->setSelectedSketchElement(id);
                syncProperties();
                statusBar()->showMessage(tr("Skizzenelement gewählt"), 2000);
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
    const auto features = impl_->session.features();
    impl_->modelTree->showModel(impl_->session.modelOutline(), features,
                                impl_->session.selectedFeature(), impl_->visibility);
    syncTimeline();
}

void MainWindow::syncProperties() {
    impl_->pendingParameter = {};
    impl_->pendingStamp.reset();
    impl_->propertyPanel->setErrorCommitHandler({});
    const ShellState shell = impl_->shell();
    const std::optional<document::FeatureId> selected = shell.feature;
    impl_->repairAllAction->setEnabled(!impl_->session.features().empty());
    impl_->repairAction->setEnabled(shell.canEditSketch());
    if (!selected.has_value()) {
        impl_->propertyPanel->showNone();
        syncTransformTool();
        syncShell();
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
    syncShell();
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
    syncVisibility();
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
    syncTimeline();
    syncProperties();
    syncSketchPresentation();
}

} // namespace voelund::desktop
