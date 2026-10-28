// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QMainWindow>
#include <functional>
#include <voelund/application/CommandTransaction.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <voelund/document/BodyAttributes.hpp>
#include <voelund/document/BodyId.hpp>

class QCloseEvent;
class TopoDS_Shape;

namespace voelund::application {
class CadSession;
struct ActionResult;
struct DeletionPlan;
} // namespace voelund::application

namespace voelund::document {
enum class FeatureKind;
} // namespace voelund::document

namespace voelund::modules::solid {
struct SolidCommandServices;
}

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
    void createSectionView();
    void syncShownSketches();                                 // MainWindowView.cpp
    [[nodiscard]] bool selectedSketchHidden() const;          // N05
                                                              // (MainWindowView.cpp)
    void showTreeMenu(int kind, std::uint64_t id, QPoint at); // W09 (MainWindowView.cpp)
    // W06 (MainWindowMoveValues.cpp): the dragged distance (or, with an axis, the angle in
    // delta[0]) in a field at the pointer; a typed value makes it exact.
    void offerMoveValue(document::FeatureId feature, std::array<double, 3> delta,
                        std::optional<int> axis);
    // M01: bodies as a whole (MainWindowBodies.cpp).
    void createBodyCommands();
    void createBodyAppearance(); // N06: colour card and display style (MainWindowBodies.cpp)
    void colorBodies(std::optional<std::uint32_t> color);
    void giveMaterial(std::optional<document::BodyMaterial> material); // N07
    void showMassProperties();                                         // N07
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
    void selectBodiesInBox(const std::vector<std::uint64_t>& shown, bool additive); // W07
    void clearSelectionEverywhere();
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
    [[nodiscard]] bool canCombineBody() const;
    [[nodiscard]] bool canTreatEdges() const;
    void pushPullPickedFace();
    [[nodiscard]] bool canPushPullNow() const;
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
    void pickEdgeChain(std::uint64_t body, const TopoDS_Shape& edge); // W07
    bool toolTakesEdge(std::uint64_t body, const TopoDS_Shape& edge);
    void connectEdgePicking();
    void createInspectTools();                                               // M12, N08
    void addInspectServices(modules::solid::SolidCommandServices& services); // M12, N08
    void checkInterference();                                                // N08
    void addHoleServices(modules::solid::SolidCommandServices& services);    // M08, N09b
    [[nodiscard]] std::optional<document::FeatureId> sketchForHoles() const; // N09b
    void rollToEdited();                                                     // W08
    void restoreEditRollback();                                              // W08
    void clearEdgeSelection();
    bool beginToolEdit(document::FeatureId selected, document::FeatureKind kind); // N12a
    [[nodiscard]] std::vector<document::FeatureId> repeatableSteps() const;       // N11b
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
