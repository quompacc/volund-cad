#pragma once
#include "AutosaveController.hpp"
#include "RecoveryStore.hpp"
#include "CommandActions.hpp"
#include "CommandPalette.hpp"
#include "ContextBar.hpp"
#include "DocumentActions.hpp"
#include "FeaturePropertiesPanel.hpp"
#include "FloatingCard.hpp"
#include "ShellHeader.hpp"
#include "ShellState.hpp"
#include "PushPullToolController.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "FeatureErrorCard.hpp"
#include "StaleBanner.hpp"
#include "SketchToolActions.hpp"
#include "Theme.hpp"
#include "TimelineBar.hpp"
#include "ToolPanelHost.hpp"
#include "ToolRail.hpp"
#include "ViewCube.hpp"
#include "ViewVisibility.hpp"
#include <QAction>
#include <QElapsedTimer>
#include <functional>
#include <memory>
#include <optional>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/assistant/Assistant.hpp>
#include <voelund/desktop/McpAccess.hpp>
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
        else if ((sketchExtrusionTool && sketchExtrusionTool->isActive()) ||
                 (pushPullTool && pushPullTool->isActive()))
            inputs.tool = ShellTool::extrusion; // Its panel carries the controls (T04c).
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
    FloatingCard* propertyCard{nullptr};
    FeatureErrorCard* errorCard{nullptr};
    StaleBanner* staleBanner{nullptr};
    ToolPanelHost* toolPanel{nullptr};
    std::unique_ptr<SketchEditorToolController> sketchEditorTool;
    std::unique_ptr<SketchExtrusionToolController> sketchExtrusionTool;
    std::unique_ptr<PushPullToolController> pushPullTool; // T04c
    ShellHeader* header{nullptr};
    ViewCube* viewCube{nullptr};
    QAction* themeAction{nullptr};
    ThemeMode themeMode{ThemeMode::light};
    ToolRail* toolRail{nullptr};
    ContextBar* contextBar{nullptr};
    CommandPalette* commandPalette{nullptr};
    QAction* searchAction{nullptr};
    QAction* toolRailAction{nullptr};
    QAction* undoAction{nullptr};
    QAction* redoAction{nullptr};
    QAction* repairAction{nullptr};
    QAction* repairAllAction{nullptr};
    QAction* addBoxAction{nullptr};
    QAction* addCylinderAction{nullptr};
    QAction* sketchEditorAction{nullptr};
    QAction* transformAction{nullptr};
    QAction* snapAction{nullptr};
    // S13: view switches as commands (grid everywhere, the others in the sketch editor).
    QAction* gridAction{nullptr};
    std::vector<std::pair<std::string, QAction*>> sketchSwitchActions;
    QDoubleSpinBox* linearSnapInput{nullptr};
    QDoubleSpinBox* angularSnapInput{nullptr};
    std::optional<viewport::PlanarFaceSelection> selectedPlanarFace;
    std::optional<document::DatumPlane> selectedDatum; // V01b: base plane picked in viewport.
    ViewVisibility visibility;                         // U06: model tree eyes (view state)
    TimelineBar* timeline{nullptr};
    std::unique_ptr<SketchToolActions> sketchActions; // U13
    workspaces::CommandRegistry commandRegistry;
    std::unique_ptr<CommandActions> commandActions;
    std::unique_ptr<DocumentActions> documentActions;
    // L05: recovery entries and the autosave that writes them.
    std::unique_ptr<RecoveryStore> recoveryStore;
    std::unique_ptr<AutosaveController> autosave;
    // AI04c: local Claude access (MCP).
    std::unique_ptr<McpAccess> mcp;
    QAction* mcpAction{nullptr};
    QAction* mcpCopyAction{nullptr};
    QAction* mcpTokenAction{nullptr};
    QAction* aiAllowedAction{nullptr}; // Privacy switch for all AI functions.
    // AI03b: Claude panel and its backend (Claude Code against the server above).
    QAction* assistantPanelAction{nullptr};
    QDockWidget* assistantDock{nullptr};
    class AssistantPanel* assistantPanel{nullptr};
    std::unique_ptr<assistant::ClaudeCodeBackend> assistantBackend;
    std::uint64_t assistantGeneration{0};
    QLabel* mcpAddress{nullptr};
    QElapsedTimer mcpActivity; // Last accepted request; invalid until the first one.
};

} // namespace voelund::desktop
