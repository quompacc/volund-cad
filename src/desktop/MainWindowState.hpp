#pragma once
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "ShellState.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "ToolPanelHost.hpp"
#include "WorkspaceToolbar.hpp"
#include <QAction>
#include <functional>
#include <optional>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/workspaces/CommandRegistry.hpp>
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
namespace voelund::desktop {
class ModelTreePanel;
class MainWindow::Impl final {
  public:
    explicit Impl(application::CadSession& cadSession) : session(cadSession) {}

    // The single projection all shell widgets read (U02, docs/desktop-shell.md).
    [[nodiscard]] ShellState shell() const {
        ShellInputs inputs;
        inputs.basePlane = selectedDatum;
        inputs.planarFace = selectedPlanarFace.has_value();
        if (sketchEditorTool && sketchEditorTool->isActive())
            inputs.tool = ShellTool::sketchEditor;
        else if (sketchExtrusionTool && sketchExtrusionTool->isActive())
            inputs.tool = ShellTool::extrusion;
        else if (transformAction && transformAction->isChecked())
            inputs.tool = ShellTool::transform;
        return projectShell(session, inputs);
    }

    application::CadSession& session;
    QLabel* geometryStatus{nullptr};
    std::function<application::CommandResult(application::ErrorDisposition)> pendingParameter;
    std::optional<application::DocumentStamp> pendingStamp;
    std::optional<document::FeatureId> pendingSelection;
    viewport::CadViewport* viewport{nullptr};
    ModelTreePanel* modelTree{nullptr};
    FeaturePropertiesPanel* propertyPanel{nullptr};
    QDockWidget* propertyDock{nullptr};
    ToolPanelHost* toolPanel{nullptr};
    std::unique_ptr<SketchEditorToolController> sketchEditorTool;
    std::unique_ptr<SketchExtrusionToolController> sketchExtrusionTool;
    QAction* undoAction{nullptr};
    QAction* redoAction{nullptr};
    QAction* repairAction{nullptr};
    QAction* repairAllAction{nullptr};
    QAction* addBoxAction{nullptr};
    QAction* addCylinderAction{nullptr};
    QAction* sketchEditorAction{nullptr};
    QAction* transformAction{nullptr};
    QAction* snapAction{nullptr};
    QDoubleSpinBox* linearSnapInput{nullptr};
    QDoubleSpinBox* angularSnapInput{nullptr};
    std::optional<viewport::PlanarFaceSelection> selectedPlanarFace;
    std::optional<document::DatumPlane> selectedDatum; // V01b: base plane picked in viewport.
    workspaces::CommandRegistry commandRegistry;
    std::unique_ptr<WorkspaceToolbar> workspaceToolbar;
    std::unique_ptr<DocumentActions> documentActions;
};

} // namespace voelund::desktop
