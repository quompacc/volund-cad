#pragma once

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

// Index of "Rechteck (Mitte)" in the tool choice (S04a).
inline constexpr int centerRectangleTool = 10;
inline constexpr int pointTool = 11; // S15

class ToolPanelHost;

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
    [[nodiscard]] bool isActive() const noexcept;
    // S17: while on, a body edge clicked in the viewport is projected into the sketch.
    [[nodiscard]] bool projecting() const noexcept { return editor_ && projecting_; }
    void setProjecting(bool on);
    void projectEdge(std::uint64_t bodyResult, const TopoDS_Shape& edge);
    void cancel();

  private:
    friend class SketchSolverGateTestAccess;
    void setTool(int index);
    void applyTranslation(const std::string& id, double value);
    [[nodiscard]] bool dragTo(double translationX, double translationY);
    void applyElementDimension(const std::string& id, double value);
    void applyLineConstraint(document::SketchLineConstraint constraint);
    void applyRelation(document::SketchConstraintKind kind);
    void toggleConstruction();
    void lockSelectedReference();
    void unlockSelectedReference();
    void removeSelected();
    void removeLastConstraint();
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
    double offsetDistance_{1.0};
    double patternCount_{3.0};
    double patternSpacing_{10.0};
    double patternAngle_{360.0};
    int toolIndex_{1};
    bool editing_{false};
    bool automaticRelations_{true}; // S04: kept for the next sketch
    bool showDimensions_{true};     // S13: view switches, kept for the next sketch
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
};

} // namespace voelund::desktop
