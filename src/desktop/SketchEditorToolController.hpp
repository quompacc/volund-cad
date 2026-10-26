// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include "SketchOverview.hpp"

#include <QElapsedTimer>
#include <QPointer>

#include <functional>
#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

inline constexpr int rectangleTool = 3; // W13
// Index of "Rechteck (Mitte)" in the tool choice (S04a).
inline constexpr int centerRectangleTool = 10;
inline constexpr int pointTool = 11;            // S15
inline constexpr int bsplineTool = 12;          // S21
inline constexpr int dimensionTool = 13;        // W02
inline constexpr int circleTwoPointTool = 14;   // W20
inline constexpr int circleThreePointTool = 15; // W20
inline constexpr int toolCount = 16;

class ToolPanelHost;
class InlineValueEdit;
struct ResolvedParameter;

class SketchEditorToolController final {
  public:
    SketchEditorToolController(application::CadSession& session, viewport::CadViewport& viewport,
                               ToolPanelHost& panel, std::function<void()> sceneChanged,
                               std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& support);
    void beginOnBasePlane(document::DatumPlane datum);
    // M19: a new sketch on a construction plane, at its current frame.
    void beginOnConstructionPlane(document::FeatureId plane);
    [[nodiscard]] bool beginSelectedSketch();
    // L05b: the open draft for its recovery file, once it has changes.
    struct RecoveryDraft final {
        std::optional<document::FeatureId> edited;
        std::optional<document::FeatureId> support;
        document::SketchParameters sketch;
        bool operator==(const RecoveryDraft&) const = default;
    };
    [[nodiscard]] std::optional<RecoveryDraft> recoveryDraft() const;
    // L05b: opens a recovered draft the way it was started (edited sketch, base plane or
    // support face) and loads its content; false leaves no editor open.
    [[nodiscard]] bool restoreDraft(const application::CadSession::SketchDraft& draft,
                                    std::optional<document::FeatureId> edited);
    void updatePreview(const std::optional<viewport::SketchElementIntent>& intent);
    void confirmElement(const viewport::SketchElementIntent& intent);
    void continuePolyline(const application::SketchCreationResult& created);
    void translateFromViewport(double deltaX, double deltaY);
    // U09: pointer-up or the end of a typed translation records the move as one step.
    void finishDrag();
    [[nodiscard]] bool selectElement(const viewport::SketchSelectionIntent& intent);
    void clearSelection();
    // S13: box selection and a relation picked at its label.
    void selectElements(const std::vector<std::uint64_t>& ids, bool additive);
    void selectConstraint(std::uint64_t id);
    // S13: the four switches ("relations", "show_grid", "show_dimensions",
    // "show_projected") from the panel or a command; the grid belongs to the window.
    void setGridSwitch(std::function<bool()> shown, std::function<void(bool)> show);
    // Called after a switch changed, so the window keeps its commands in step.
    void setSwitchHandler(std::function<void()> handler) { switchChanged_ = std::move(handler); }
    void setViewSwitch(const std::string& id, bool on);
    [[nodiscard]] bool viewSwitch(const std::string& id) const;
    // The panel's grid choice follows a change from the model tree.
    void syncGridChoice();
    void editConstraint(std::uint64_t constraintId);
    // PAR02: "=Name" (or a number, nullopt) in the length or radius field of an element.
    void bindElementDimension(const std::string& id, std::optional<ResolvedParameter> bound);
    [[nodiscard]] bool isActive() const noexcept;
    // S17: while on, a body edge clicked in the viewport is projected into the sketch.
    [[nodiscard]] bool projecting() const noexcept { return editor_ && projecting_; }
    void setProjecting(bool on);
    void projectEdge(std::uint64_t bodyResult, const TopoDS_Shape& edge);
    void cancel();

  private:
    // D02 (SketchEditorDimensionEdit.cpp): the value field at the pointer and what it applies;
    // a refused text comes back with its reason.
    void openDimensionEdit(std::uint64_t constraintId, const QString& text);
    [[nodiscard]] std::optional<QString> applyDimensionText(std::uint64_t constraintId,
                                                            const QString& text);
    std::optional<QString> bindDimensionText(document::SketchConstraintId id, const QString& name,
                                             bool angle, double current);
    std::optional<QString> setDimensionNumber(document::SketchConstraintId id, double value,
                                              bool angle, double current);
    InlineValueEdit* valueEdit_{nullptr};
    QElapsedTimer lastIdleEscape_; // W10: two quick Escapes with nothing to step back finish
    // W02 (SketchDimensionTool.cpp): a click of the dimension tool; the single dimension of the
    // first pick, while nothing is typed, gives way to one with the second pick.
    bool pickForDimension(const viewport::SketchSelectionIntent& intent);
    std::optional<document::SketchConstraintId> pairWithFirst(document::SketchElementId element,
                                                              document::SketchPoint at);
    std::optional<std::uint64_t> dimensionAnchor_;
    std::optional<std::pair<document::SketchElementId, document::SketchPoint>> dimensionFirst_;
    std::optional<document::SketchConstraintId>
    singleDimensionFor(document::SketchElementId element);
    // D01 (SketchTypedDimensions.cpp): typed values as dimensions of the new element; the
    // fields at the pointer evaluate "=Name*2" with the document's parameters.
    void applyTypedDimensions(const application::SketchCreationResult& created,
                              const std::vector<viewport::SketchTypedDimension>& typed);
    void installDynamicInputEvaluator();
    friend class SketchSolverGateTestAccess;
    void setTool(int index);
    void applyTranslation(const std::string& id, double value);
    void applyParameter(const std::string& id, double value); // SketchEditorParameters.cpp
    [[nodiscard]] bool dragTo(double translationX, double translationY);
    void applyElementDimension(const std::string& id, double value);
    void applyLineConstraint(document::SketchLineConstraint constraint);
    void applyRelation(document::SketchConstraintKind kind);
    void toggleConstruction();
    void lockSelectedReference();
    void unlockSelectedReference();
    void removeSelected();
    void removeLastConstraint();
    void toggleReference(); // S18
    void applyEditOperation(const std::string& id);
    void applyPatternOperation(const std::string& id);
    void applyCurveEdit(const std::function<application::SketchCurveEditResult()>& edit);
    void resetTranslationInputs();
    void syncSelectedDimensionInputs();
    void syncConstraintState();
    void showPanel();
    void refreshPresentation();
    void accept(application::ErrorDisposition disposition = application::ErrorDisposition::reject);
    void stepBack();
    // S13: Cancel asks before discarding changes; Enter ends a polyline.
    void requestCancel();
    bool removeSelectedConstraint();
    void applyViewSwitch(const std::string& id, bool on);
    // SketchEditorActions.cpp: the editor's commands by their panel and shortcut IDs.
    void runAction(const std::string& id);
    void stepHistory(bool back);
    void chooseOption(const std::string& id, int index);
    void applyRectangleOptions(const application::SketchCreationResult& created,
                               const viewport::SketchElementIntent& intent);
    void syncRectangleOptions();
    [[nodiscard]] bool
    hiddenAnnotation(const features::SketchConstraintAnnotation& annotation) const;
    void finishStep();
    void open(std::unique_ptr<application::SketchEditorSession> editor,
              const viewport::PlanarFaceSelection& drawing,
              std::optional<document::FeatureId> edited,
              std::optional<document::FeatureId> support);
    void startOnSupport(const viewport::PlanarFaceSelection& drawing, document::FeatureId support,
                        const document::SketchPlane& plane,
                        const document::SketchSupportBinding& binding);

    application::CadSession& session_;
    double dragBaseX_{0.0};
    double dragBaseY_{0.0};
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::unique_ptr<application::SketchEditorSession> editor_;
    double translationX_{0.0};
    double translationY_{0.0};
    double filletRadius_{2.0};
    double chamferDistance_{2.0};                     // S19
    std::vector<document::SketchPoint> bsplinePoles_; // S21: clicked so far
    document::SketchPoint transformCenter_{};         // S20
    double rotateAngle_{90.0};
    double scaleFactor_{2.0};
    double offsetDistance_{1.0};
    double patternCount_{3.0};
    double patternSpacing_{10.0};
    double patternAngle_{360.0};
    int toolIndex_{1};
    bool editing_{false};
    bool automaticRelations_{true}; // S04: kept for the next sketch
    application::SketchEditorSession::RectangleOptions rectangleOptions_{}; // W13
    bool showDimensions_{true}; // S13: view switches, kept for the next sketch
    bool showProjected_{true};
    bool projecting_{false}; // S17
    std::function<bool()> gridShown_;
    std::function<void(bool)> showGrid_;
    std::function<void()> switchChanged_;
    // L05b: how the open editor was started, for its recovery draft.
    std::optional<document::FeatureId> draftEdited_;
    std::optional<document::FeatureId> draftSupport_;
    std::optional<viewport::SketchAutomaticConstraint> previewAutomatic_;
    // S11: elements a refused edit points at; red until the selection changes.
    std::vector<std::uint64_t> problemElements_;
    QPointer<SketchOverview> overview_; // W19: owned by the panel while the sketch is open
    void syncOverview();
    void tieLooseEnds();
    [[nodiscard]] std::optional<viewport::SketchElementIntent>
    drawn(const viewport::SketchElementIntent& clicks) const;
    [[nodiscard]] bool circleVariant() const noexcept {
        return toolIndex_ == circleTwoPointTool || toolIndex_ == circleThreePointTool;
    }
    // W20: a right click in the sketch ends the drawing tool or opens the sketch's menu there.
    void contextRequested(QPoint globalPosition);
    // W21: a dimension's line follows its dragged value; the release is one undo step.
    void dragDimension(std::uint64_t id, double x, double y, bool finished);
};

} // namespace voelund::desktop
