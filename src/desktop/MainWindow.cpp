#include <voelund/desktop/MainWindow.hpp>

#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "ModelTreePanel.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"
#include "WorkspaceToolbar.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>
#include <voelund/viewport/CadViewport.hpp>
#include <voelund/workspaces/CommandRegistry.hpp>

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
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

class MainWindow::Impl final {
  public:
    explicit Impl(application::CadSession& cadSession) : session(cadSession) {}

    application::CadSession& session;
    viewport::CadViewport* viewport{nullptr};
    ModelTreePanel* modelTree{nullptr};
    FeaturePropertiesPanel* propertyPanel{nullptr};
    ToolPanelHost* toolPanel{nullptr};
    std::unique_ptr<SketchEditorToolController> sketchEditorTool;
    std::unique_ptr<SketchExtrusionToolController> sketchExtrusionTool;
    QAction* undoAction{nullptr};
    QAction* redoAction{nullptr};
    QAction* addBoxAction{nullptr};
    QAction* addCylinderAction{nullptr};
    QAction* sketchEditorAction{nullptr};
    QAction* createSketchAction{nullptr};
    QAction* transformAction{nullptr};
    QAction* snapAction{nullptr};
    QDoubleSpinBox* linearSnapInput{nullptr};
    QDoubleSpinBox* angularSnapInput{nullptr};
    QComboBox* connectionModeInput{nullptr};
    std::optional<viewport::PlanarFaceSelection> selectedPlanarFace;
    workspaces::CommandRegistry commandRegistry;
    std::unique_ptr<WorkspaceToolbar> workspaceToolbar;
    std::unique_ptr<DocumentActions> documentActions;
};

MainWindow::MainWindow(application::CadSession& session, QWidget* parent)
    : QMainWindow(parent), impl_(std::make_unique<Impl>(session)) {
    createViewport();
    createModelTree();
    createPropertiesPanel();
    createToolPanel();
    createHistoryToolbar();
    createWorkspaceToolbar();
    createConnectionToolbar();
    createTransformSettingsToolbar();
    impl_->documentActions =
        std::make_unique<DocumentActions>(*this, impl_->session, [this]() { refreshScene(); });
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
        static_cast<void>(impl_->session.addBoxFeature());
        refreshScene();
        statusBar()->showMessage(tr("Box feature created"), 2000);
    };
    services.addCylinder = [this]() {
        static_cast<void>(impl_->session.addCylinderFeature(
            resultModeFromIndex(impl_->connectionModeInput->currentIndex())));
        refreshScene();
        statusBar()->showMessage(tr("Cylinder feature created"), 2000);
    };
    services.sketchEditor = [this]() {
        if (impl_->selectedPlanarFace.has_value()) {
            if (impl_->sketchExtrusionTool->isActive()) {
                impl_->sketchExtrusionTool->cancel();
            }
            impl_->sketchEditorTool->begin(*impl_->selectedPlanarFace);
        }
    };
    services.sketchRectangle = [this]() {
        if (!impl_->selectedPlanarFace.has_value()) {
            return;
        }
        if (impl_->sketchEditorTool->isActive()) {
            impl_->sketchEditorTool->cancel();
        }
        impl_->sketchExtrusionTool->begin(*impl_->selectedPlanarFace);
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
    services.canEditSketch = [this]() { return impl_->selectedPlanarFace.has_value(); };
    services.canSketch = [this]() { return impl_->selectedPlanarFace.has_value(); };
    services.canEditExistingSketch = [this]() {
        const auto selected = impl_->session.selectedFeature();
        return selected.has_value() &&
               impl_->session.selectedFeatureDescriptor().kind == document::FeatureKind::sketch;
    };
    services.canExtrudeProfiles = [this]() {
        return impl_->session.canExtrudeSelectedSketchProfiles();
    };
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
    impl_->sketchEditorAction = impl_->workspaceToolbar->action("solid.create.sketch");
    impl_->createSketchAction = impl_->workspaceToolbar->action("solid.create.sketch_rectangle");
    impl_->transformAction = impl_->workspaceToolbar->action("solid.modify.transform");
    impl_->transformAction->setObjectName(QStringLiteral("transformAction"));
}

void MainWindow::createToolPanel() {
    impl_->toolPanel = new ToolPanelHost(this);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolPanel);
    impl_->sketchEditorTool = std::make_unique<SketchEditorToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->sketchExtrusionTool = std::make_unique<SketchExtrusionToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
}

void MainWindow::createPropertiesPanel() {
    impl_->propertyPanel = new FeaturePropertiesPanel(this);
    auto* dock = new QDockWidget(tr("Properties"), this);
    dock->setObjectName(QStringLiteral("featurePropertiesDock"));
    dock->setWidget(impl_->propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::createHistoryToolbar() {
    impl_->undoAction = new QAction(tr("Undo"), this);
    impl_->undoAction->setShortcut(QKeySequence::Undo);
    impl_->redoAction = new QAction(tr("Redo"), this);
    impl_->redoAction->setShortcuts(QKeySequence::Redo);
    auto* toolbar = addToolBar(tr("History"));
    toolbar->setObjectName(QStringLiteral("historyToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(impl_->undoAction);
    toolbar->addAction(impl_->redoAction);
}

void MainWindow::createConnectionToolbar() {
    auto* toolbar = addToolBar(tr("Connection"));
    toolbar->setObjectName(QStringLiteral("connectionToolbar"));
    toolbar->addWidget(new QLabel(tr("Connection: "), toolbar));
    impl_->connectionModeInput = new QComboBox(toolbar);
    impl_->connectionModeInput->setObjectName(QStringLiteral("connectionModeInput"));
    impl_->connectionModeInput->addItems({tr("Join"), tr("Cut"), tr("Intersect"), tr("Separate")});
    impl_->connectionModeInput->setToolTip(
        tr("Connection mode for the current and next cylinder operation"));
    toolbar->addWidget(impl_->connectionModeInput);
    connect(impl_->connectionModeInput, &QComboBox::currentIndexChanged, this,
            [this](int) { applyCylinderResultMode(); });
}

void MainWindow::createTransformSettingsToolbar() {
    impl_->snapAction = new QAction(tr("Snap"), this);
    impl_->snapAction->setCheckable(true);
    impl_->snapAction->setChecked(true);
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
            impl_->selectedPlanarFace = selection;
            impl_->workspaceToolbar->refreshEnabled();
            statusBar()->showMessage(tr("Planar face selected"), 2000);
        });
    impl_->viewport->setSketchRectangleHandler(
        [this](const viewport::SketchRectangleResult& rectangle) {
            impl_->sketchExtrusionTool->setResult(rectangle);
        });
    impl_->viewport->setSketchExtrusionPreviewHandler(
        [this](double distance) { impl_->sketchExtrusionTool->updateDistancePreview(distance); });
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
            if (impl_->session.setSelectedFeaturePlacement(placement)) {
                refreshScene();
                statusBar()->showMessage(tr("Feature moved"), 2000);
            }
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
    impl_->viewport->setBodyRotationHandler([this](std::uint64_t id, int axisIndex,
                                                   double angleDegrees) {
        if (!selectTransformFeatureForBody(impl_->session, document::FeatureId{id})) {
            return;
        }
        kernel::RigidTransform placement = impl_->session.selectedFeaturePlacement();
        double* rotations[] = {&placement.rotationX, &placement.rotationY, &placement.rotationZ};
        if (axisIndex < 0 || axisIndex > 2) {
            return;
        }
        *rotations[axisIndex] += angleDegrees;
        if (impl_->session.setSelectedFeaturePlacement(placement)) {
            refreshScene();
            statusBar()->showMessage(tr("Feature rotated"), 2000);
        }
    });
    impl_->viewport->setBodyRotationPreviewHandler(
        [this](std::uint64_t id, int axisIndex, double angleDegrees) {
            previewBodyRotation(id, axisIndex, angleDegrees);
        });
    connect(impl_->undoAction, &QAction::triggered, this, [this]() {
        if (impl_->session.undo()) {
            refreshScene();
        }
    });
    connect(impl_->redoAction, &QAction::triggered, this, [this]() {
        if (impl_->session.redo()) {
            refreshScene();
        }
    });
}

void MainWindow::syncModelTree() {
    impl_->modelTree->showFeatures(impl_->session.features(), impl_->session.selectedFeature());
}

void MainWindow::syncProperties() {
    const std::optional<document::FeatureId> selected = impl_->session.selectedFeature();
    impl_->workspaceToolbar->refreshEnabled();
    if (const auto cylinderId = latestCylinderId(impl_->session)) {
        const QSignalBlocker blocker(impl_->connectionModeInput);
        impl_->connectionModeInput->setCurrentIndex(
            resultModeIndex(impl_->session.cylinderParameters(*cylinderId).resultMode));
    }
    impl_->connectionModeInput->setEnabled(true);
    if (!selected.has_value()) {
        impl_->propertyPanel->showNone();
        impl_->undoAction->setEnabled(false);
        impl_->redoAction->setEnabled(false);
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
    impl_->undoAction->setEnabled(impl_->session.canUndo());
    impl_->redoAction->setEnabled(impl_->session.canRedo());
    syncTransformTool();
    impl_->documentActions->syncWindowTitle();
}

void MainWindow::syncTreeStatus() {
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
    impl_->viewport->cancelSketch();
    impl_->toolPanel->end();
    const std::vector<viewport::SceneBody> bodies = makeSceneBodies(impl_->session);
    impl_->viewport->setBodies(bodies);
    syncModelTree();
    syncProperties();
    syncTreeStatus();
    syncSketchPresentation();
}

void MainWindow::applyBoxParameters() {
    if (impl_->session.setSelectedBoxParameters(impl_->propertyPanel->boxParameters())) {
        refreshScene();
        statusBar()->showMessage(tr("Feature graph recomputed"), 2000);
    }
}

void MainWindow::applyCylinderParameters() {
    const document::CylinderParameters parameters = impl_->propertyPanel->cylinderParameters(
        impl_->session.selectedCylinderParameters().resultMode);
    if (impl_->session.setSelectedCylinderParameters(parameters)) {
        refreshScene();
        const QString errorText =
            featureErrorText(impl_->session, impl_->session.selectedFeature().value());
        statusBar()->showMessage(errorText.isEmpty() ? tr("Feature graph recomputed") : errorText,
                                 errorText.isEmpty() ? 2000 : 5000);
    }
}

void MainWindow::applyCylinderResultMode() {
    const std::optional<document::FeatureId> cylinderId = latestCylinderId(impl_->session);
    if (!cylinderId.has_value()) {
        statusBar()->showMessage(tr("Connection mode set for the next cylinder"), 2000);
        return;
    }
    document::CylinderParameters parameters = impl_->session.cylinderParameters(*cylinderId);
    parameters.resultMode = resultModeFromIndex(impl_->connectionModeInput->currentIndex());
    if (impl_->session.setCylinderParameters(*cylinderId, parameters)) {
        refreshScene();
        const QString errorText = featureErrorText(impl_->session, *cylinderId);
        statusBar()->showMessage(errorText.isEmpty() ? tr("Connection mode changed") : errorText,
                                 errorText.isEmpty() ? 2000 : 5000);
    }
}

void MainWindow::applyExtrusionParameters() {
    const document::ExtrusionParameters parameters = impl_->propertyPanel->extrusionParameters();
    if (impl_->session.setSelectedExtrusionParameters(parameters)) {
        refreshScene();
        const QString errorText =
            featureErrorText(impl_->session, impl_->session.selectedFeature().value());
        statusBar()->showMessage(errorText.isEmpty() ? tr("Extrusion recomputed") : errorText,
                                 errorText.isEmpty() ? 2000 : 5000);
    }
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
