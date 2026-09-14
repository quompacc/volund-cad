#pragma once
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "ToolPanelHost.hpp"
#include "WorkspaceToolbar.hpp"
#include <functional>
#include <optional>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/workspaces/CommandRegistry.hpp>
class QAction;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
namespace voelund::desktop {
class ModelTreePanel;
class MainWindow::Impl final {
  public:
    explicit Impl(application::CadSession& cadSession) : session(cadSession) {}

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

} // namespace voelund::desktop
