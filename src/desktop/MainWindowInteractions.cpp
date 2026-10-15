#include <voelund/desktop/MainWindow.hpp>
#include "AssistantPanel.hpp"
#include "CommandFeedback.hpp"
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "ModelTreePanel.hpp"
#include "ShellHeader.hpp"
#include "ShellActions.hpp"
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
#include <algorithm>
#include <array>
#include <cmath>
#include <QWidget>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

// Viewport, tree and property-panel interactions of the main window.
namespace voelund::desktop {

void MainWindow::connectInteractions() {
    impl_->propertyPanel->setBoxChangeHandler([this]() { applyBoxParameters(); });
    impl_->propertyPanel->setCylinderChangeHandler([this]() { applyCylinderParameters(); });
    impl_->propertyPanel->setExtrusionChangeHandler([this]() { applyExtrusionParameters(); });
    impl_->modelTree->setHandlers({[this](std::uint64_t id) { selectFeature(id); },
                                   [this](int plane) { selectBasePlane(plane); },
                                   [this](ModelTreePanel::Kind kind, std::uint64_t id) {
                                       toggleVisibility(static_cast<int>(kind), id);
                                   },
                                   [this](std::uint64_t id) { selectBody(id); },
                                   [this](std::uint64_t id, const QString& name) {
                                       renameBody(id, name);
                                   }});
    impl_->viewport->setBodySelectionHandler([this](std::uint64_t id) {
        if (!impl_->combineTool->isActive()) // M21: the panel takes clicks instead
            selectFeature(id);
    });
    impl_->viewport->setBodyClickHandler([this](std::uint64_t id) {
        if (impl_->combineTool->isActive()) // M21: a click adds or removes a tool body
            impl_->combineTool->picked(document::FeatureId{id});
    });
    impl_->viewport->setPlanarFaceSelectionHandler(
        [this](const viewport::PlanarFaceSelection& selection) {
            if (impl_->pushPullTool->isActive() || impl_->combineTool->isActive())
                return; // T04c: the face of a running push/pull stays.
            if (impl_->holeTool->isActive())
                return impl_->holeTool->place(selection); // M08: moves the hole there
            if (impl_->shellTool->isActive())
                return impl_->shellTool->picked(selection); // M22: opens or closes the face
            if (impl_->draftTool->isActive())
                return impl_->draftTool->picked(selection); // M25: tilts the face or not
            if (impl_->mirrorTool->isActive())
                return impl_->mirrorTool->picked(selection); // M09: the mirror plane
            clearEdgeSelection();
            // T03: while the extrusion waits for its end face, the pick goes to the tool.
            if (impl_->sketchExtrusionTool->awaitsEndFace()) {
                impl_->sketchExtrusionTool->pickEndFace(document::FeatureId{selection.bodyId},
                                                        selection.selectedFace);
                return;
            }
            impl_->selectedDatum.reset();
            impl_->selectedBody.reset();
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
    impl_->viewport->setDatumSelectionHandler([this](std::uint64_t id) { // M19
        if (impl_->shell().toolBlocksCommands())
            return;
        impl_->selectedDatum.reset();
        impl_->selectedBody.reset();
        impl_->selectedPlanarFace.reset();
        selectFeature(id);
        syncShell();
    });
    // A click into empty space selects nothing: no plane, face or feature (user report).
    impl_->viewport->setEmptySelectionHandler([this] {
        if (impl_->sketchEditorTool->isActive()) {
            impl_->sketchEditorTool->clearSelection();
            return;
        }
        if (impl_->shell().toolBlocksCommands())
            return;
        clearEdgeSelection();
        impl_->selectedDatum.reset();
        impl_->selectedBody.reset();
        impl_->selectedPlanarFace.reset();
        impl_->session.clearSelection();
        syncModelTree();
        syncTimeline();
        syncProperties();
        syncSketchPresentation();
    });
    impl_->viewport->setSketchEditorPreviewHandler(
        [this](const std::optional<viewport::SketchElementIntent>& intent) {
            impl_->sketchEditorTool->updatePreview(intent);
        });
    impl_->viewport->setSketchEditorElementHandler(
        [this](const viewport::SketchElementIntent& intent) {
            impl_->sketchEditorTool->confirmElement(intent);
        });
    impl_->viewport->setSketchEditorDragEndHandler(
        [this] { impl_->sketchEditorTool->finishDrag(); });
    impl_->viewport->setSketchEditorTranslationHandler([this](double deltaX, double deltaY) {
        impl_->sketchEditorTool->translateFromViewport(deltaX, deltaY);
    });
    impl_->viewport->setSketchConstraintEditHandler(
        [this](std::uint64_t id) { impl_->sketchEditorTool->editConstraint(id); });
    impl_->viewport->setSketchConstraintSelectionHandler(
        [this](std::uint64_t id) { impl_->sketchEditorTool->selectConstraint(id); });
    impl_->viewport->setSketchBoxSelectionHandler(
        [this](const std::vector<std::uint64_t>& ids, bool additive) {
            impl_->sketchEditorTool->selectElements(ids, additive);
        });
    // A picked sketch region shows its boundary elements as selected.
    const auto showSelectedProfiles = [this] {
        // S14 (user report 2026-10-10): the picked region's sketch itself is shown, not only
        // the selection; picked in the view (not in the tree) it stayed invisible.
        syncSketchPresentation();
        impl_->selectedDatum.reset();
        impl_->selectedBody.reset();
        impl_->selectedPlanarFace.reset();
        impl_->modelTree->selectFeature(*impl_->session.selectedFeature());
        syncTimeline();
        syncProperties();
        syncShell();
        statusBar()->showMessage(tr("Skizzenbereich gewählt"), 2000);
    };
    // A click into the area of a visible sketch region picks it (user report 2026-10-05).
    impl_->viewport->setAreaClickHandler([this, showSelectedProfiles](const viewport::ViewRay& ray) {
        // M04: with the extrusion panel open, a click into a region of its sketch toggles it.
        if (impl_->sketchExtrusionTool->regionSketch()) {
            if (!impl_->sketchExtrusionTool->toggleRegionAt(ray))
                return false;
            syncSketchPresentation();
            return true;
        }
        if (impl_->shell().toolBlocksCommands() ||
            !pickSketchArea(impl_->session, ray, impl_->visibility.hiddenBodies))
            return false;
        showSelectedProfiles();
        return true;
    });
    impl_->viewport->setSketchElementSelectionHandler(
        [this, showSelectedProfiles](const viewport::SketchSelectionIntent& intent) {
            if (impl_->sketchEditorTool->isActive()) {
                static_cast<void>(impl_->sketchEditorTool->selectElement(intent));
                return;
            }
            const std::uint64_t id = intent.elementId;
            // M04: the open extrusion panel owns the region selection; a boundary click
            // toggles the region it bounds and never selects anything else.
            if (impl_->sketchExtrusionTool->isActive()) {
                if (impl_->sketchExtrusionTool->toggleRegionByElement(
                        document::SketchElementId{id}))
                    syncSketchPresentation();
                return;
            }
            if (impl_->session.selectSketchProfileByElement(document::SketchElementId{id},
                                                            intent.additive)) {
                showSelectedProfiles();
                return;
            }
            if (!intent.additive &&
                impl_->session.selectSketchElement(document::SketchElementId{id})) {
                impl_->viewport->setSelectedSketchElement(id);
                syncProperties();
                statusBar()->showMessage(tr("Skizzenelement gewählt"), 2000);
            }
        });
    // M11: a drag moves any body (CadSession::moveBody picks the primitive's placement, the
    // body's last move or a new one); one drag is one undo step.
    impl_->viewport->setBodyMoveHandler(
        [this](std::uint64_t id, double deltaX, double deltaY, double deltaZ) {
            if (!selectTransformFeatureForBody(impl_->session, document::FeatureId{id})) {
                return;
            }
            const auto feature = *impl_->session.selectedFeature();
            applyParameterCommand(
                [this, feature, deltaX, deltaY, deltaZ](application::ErrorDisposition disposition) {
                    return impl_->session.moveBody(feature, {deltaX, deltaY, deltaZ},
                                                   std::nullopt, 0.0, disposition);
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
            if (axisIndex < 0 || axisIndex > 2) {
                return;
            }
            const auto feature = *impl_->session.selectedFeature();
            const double angle = kernel::toRadians(kernel::Degrees(angleDegrees)).value();
            applyParameterCommand(
                [this, feature, axisIndex, angle](application::ErrorDisposition disposition) {
                    return impl_->session.moveBody(feature, {0.0, 0.0, 0.0}, axisIndex, angle,
                                                   disposition);
                });
        });
    impl_->viewport->setBodyRotationPreviewHandler(
        [this](std::uint64_t id, int axisIndex, double angleDegrees) {
            previewBodyRotation(id, axisIndex, angleDegrees);
        });
    connect(impl_->undoAction, &QAction::triggered, this, [this]() { applyHistoryCommand(false); });
    connect(impl_->redoAction, &QAction::triggered, this, [this]() { applyHistoryCommand(true); });
}

} // namespace voelund::desktop
