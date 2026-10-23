// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QMainWindow>
#include <functional>
#include <voelund/application/CommandTransaction.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <voelund/document/BodyId.hpp>

class QCloseEvent;
class TopoDS_Shape;

namespace voelund::application {
class CadSession;
struct ActionResult;
struct DeletionPlan;
} // namespace voelund::application

namespace voelund::viewport {
struct PlanarFaceSelection;
} // namespace voelund::viewport

namespace voelund::desktop {

enum class ThemeMode;
struct RecoveryEntry;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(application::CadSession& session, QWidget* parent = nullptr);
    ~MainWindow() override;
    // AI04c: the session was changed from outside the shell (MCP). Views follow; open tools
    // stay open, and their stored stamp makes a later accept stale instead of overwriting.
    void refreshAfterExternalChange();
    // AI04c: the local Claude access (MCP); the choice is remembered for the next start.
    bool setMcpAccessEnabled(bool enabled);
    [[nodiscard]] QString mcpConnectCommand() const;
    // U07: a file argument (desktop association) takes the same safe path as File > Open.
    void openFile(const QString& fileName);
    // L05: offers unsaved work of an earlier, interrupted run (dialog at start).
    void offerRecovery();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void createRecovery();
    void restoreSketchDraft(const RecoveryEntry& entry);
    void repairReferences();
    void repairAllReferences();
    void syncGeometryStatus();
    void applyParameterCommand(
        std::function<application::CommandResult(application::ErrorDisposition)> command);
    void keepParameterError();
    void applyHistoryCommand(bool forward);
    void createViewport();
    void createModelTree();
    void createPropertiesPanel();
    void createToolPanel();
    void createHistoryActions();
    void createCommandSurfaces();
    void createViewSwitches();
    void createSectionView(); // N05 (MainWindowView.cpp)
    // M01: bodies as a whole (MainWindowBodies.cpp).
    void createBodyCommands();
    void createBodyAppearance(); // N06: colour card and display style (MainWindowBodies.cpp)
    void colorBodies(std::optional<std::uint32_t> color);
    void syncBodyCard(bool shown);
    void syncBodyCommands();
    void selectBody(std::uint64_t id, bool keepGroup = false);
    void renameBody(std::uint64_t id, const QString& name);
    // N02: copy and paste of bodies (MainWindowClipboard.cpp); the bodies a command acts on.
    void createClipboardCommands();
    void syncClipboardCommands();
    void copyBodies();
    void pasteBodies();
    [[nodiscard]] std::vector<document::BodyId> selectedBodies() const;
    // N02b: groups of bodies and timeline steps (MainWindowGroups.cpp).
    void createGroupSelection();
    void groupBody(document::BodyId body, bool range);
    void groupStep(document::FeatureId step, bool range);
    void clearGroups();
    void syncGroups();
    [[nodiscard]] std::vector<document::FeatureId> chosenSteps() const;
    bool moveBodyGroup(double deltaX, double deltaY, double deltaZ);
    void suppressGroup();
    void deleteGroup();
    enum class DeletionChoice { cancel, cascade, alone };
    DeletionChoice askDeletion(const QString& subject, const application::DeletionPlan& plan);
    void deleteSteps(const std::vector<document::FeatureId>& chosen,
                     const application::DeletionPlan& plan, const QString& subject);
    void syncViewSwitches();
    void syncShell();
    void createHeader();
    void applyTheme(ThemeMode mode);
    void createCommands();
    void connectInteractions();
    void pickPlanarFace(const viewport::PlanarFaceSelection& selection);
    bool toolTakesFace(const viewport::PlanarFaceSelection& selection);
    bool groupTakesFace(const viewport::PlanarFaceSelection& selection);
    void syncModelTree();
    // PAR02: the parameter card under the model tree (MainWindowDocumentParameters.cpp).
    void createParameterCard();
    void syncParameters();
    // Height of the parameter card in a left column of `column` pixels.
    [[nodiscard]] int parameterCardHeight(int column) const;
    void syncVisibility();
    void toggleVisibility(int kind, std::uint64_t id);
    void selectBasePlane(int plane);
    void createTimeline();
    void syncTimeline();
    void openTimelineStep(std::uint64_t featureId);
    void showTimelineMenu(std::uint64_t featureId, QPoint at); // M14
    void toggleSuppression();
    void moveRollback(std::optional<document::FeatureId> after);
    void deleteSelectedFeature(); // M16
    void moveFeatureAfter(document::FeatureId feature, std::optional<document::FeatureId> after);
    void syncProperties();
    void syncTreeStatus();
    void syncErrorCard(std::optional<document::FeatureId> selected);
    void syncSketchPresentation();
    void syncTransformTool();
    void refreshScene();
    void applyBoxParameters();
    void applyCylinderParameters();
    void applyExtrusionParameters();
    void previewBodyMove(std::uint64_t id, double deltaX, double deltaY, double deltaZ);
    void previewBodyRotation(std::uint64_t id, int axisIndex, double angleDegrees);
    void selectFeature(std::uint64_t rawId, bool keepGroup = false);
    // User request 2026-10-13: edges picked before a fillet or chamfer.
    void pickEdge(std::uint64_t body, const TopoDS_Shape& edge, bool additive);
    void clearEdgeSelection();
    void createAssistant();
    void createAssistantPanel();
    void syncAssistant();
    void syncAssistantContext();
    void sendToAssistant(const QString& message, bool withSelection);
    void announceExternalChange(const application::ActionResult& result);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::desktop
