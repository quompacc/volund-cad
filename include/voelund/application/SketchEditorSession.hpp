#pragma once
#include <voelund/application/CommandTransaction.hpp>
#include <voelund/application/SketchCreation.hpp>
#include <voelund/application/SceneStatus.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SketchConstraintAnnotations.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <optional>
#include <span>
#include <vector>

namespace voelund::application {

class CadSession;

enum class SketchDrawingTool {
    select,
    line,
    polyline,
    rectangle,
    circle,
    arc,
    threePointArc,
    ellipse,
    slot,
    polygon
};
enum class SketchAxisReference { origin, xAxis, yAxis };

class SketchEditorSession final {
  public:
    SketchEditorSession(CadSession& session, document::FeatureId supportFeature,
                        document::SketchPlane plane,
                        std::optional<document::SketchSupportBinding> binding = std::nullopt);
    SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                        document::FeatureId supportFeature, document::SketchParameters parameters);
    // V01: a new sketch on a base plane, and editing an existing base-plane sketch.
    SketchEditorSession(CadSession& session, document::DatumPlane datum);
    SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                        document::SketchParameters parameters);

    void setTool(SketchDrawingTool tool) noexcept;
    [[nodiscard]] SketchDrawingTool tool() const noexcept;
    [[nodiscard]] bool setPreview(document::SketchPoint first, document::SketchPoint second,
                                  std::optional<document::SketchPoint> third = std::nullopt);
    void clearPreview() noexcept;
    [[nodiscard]] std::optional<document::SketchElementId> confirmPreview();
    // U08: confirmPreview with every created element by role.
    [[nodiscard]] std::optional<SketchCreationResult> confirmCreation();
    // U08: one creation step of the active tool with its snaps and automatic relations, as
    // the interactive editor performs it (SketchCreation.hpp). nullopt for an invalid size; a
    // rejected reference throws std::invalid_argument and leaves the editor as before (S02).
    [[nodiscard]] std::optional<SketchCreationResult> create(const SketchCreationInput& input);
    [[nodiscard]] bool selectElement(document::SketchElementId elementId) noexcept;
    [[nodiscard]] bool selectNearestReference(document::SketchElementId elementId,
                                              document::SketchPoint position,
                                              bool additive) noexcept;
    [[nodiscard]] bool translateSelected(double deltaX, double deltaY);
    [[nodiscard]] std::optional<double> selectedLineLength() const noexcept;
    [[nodiscard]] std::optional<double> selectedRectangleWidth() const noexcept;
    [[nodiscard]] std::optional<double> selectedRectangleHeight() const noexcept;
    [[nodiscard]] std::optional<double> selectedCircleRadius() const noexcept;
    [[nodiscard]] std::optional<document::SketchLineConstraint>
    selectedLineConstraint() const noexcept;
    [[nodiscard]] bool setSelectedLineLength(double length);
    [[nodiscard]] bool setSelectedRectangleWidth(double width);
    [[nodiscard]] bool setSelectedRectangleHeight(double height);
    [[nodiscard]] bool setSelectedCircleRadius(double radius);
    [[nodiscard]] bool setSelectedLineConstraint(document::SketchLineConstraint constraint);
    [[nodiscard]] bool canAddSelectedRelation(document::SketchConstraintKind kind) const noexcept;
    [[nodiscard]] bool addSelectedRelation(document::SketchConstraintKind kind);
    [[nodiscard]] bool addCoincidentReferences(document::SketchGeometryReference first,
                                               document::SketchGeometryReference second);
    [[nodiscard]] bool addAxisReference(document::SketchGeometryReference reference,
                                        SketchAxisReference axis);
    [[nodiscard]] bool addMidpointReference(document::SketchGeometryReference reference,
                                            document::SketchElementId line);
    [[nodiscard]] bool addFixedReference(document::SketchGeometryReference reference,
                                         document::SketchPoint point);
    [[nodiscard]] bool addSupportReference(document::SketchGeometryReference reference,
                                           document::SketchPoint point,
                                           std::uint64_t supportReferenceId,
                                           double supportReferenceParameter);
    [[nodiscard]] bool addProjectedEdgeReference(document::SketchGeometryReference point,
                                                 const document::ProjectedEdgeReference& reference);
    [[nodiscard]] bool addPointOnObjectReference(document::SketchGeometryReference reference,
                                                 document::SketchElementId object);
    [[nodiscard]] std::optional<document::SketchConstraintKind>
    addAutomaticLineRelation(document::SketchElementId line);
    [[nodiscard]] bool addTangencyReference(document::SketchElementId radial,
                                            document::SketchElementId target);
    [[nodiscard]] bool addAutomaticRadialRelation(document::SketchElementId radial,
                                                  document::SketchElementId target,
                                                  document::SketchConstraintKind kind);
    [[nodiscard]] bool isSelectedConstruction() const noexcept;
    [[nodiscard]] bool toggleSelectedConstruction();
    [[nodiscard]] bool canLockSelectedReference() const noexcept;
    [[nodiscard]] bool isSelectedReferenceLocked() const noexcept;
    [[nodiscard]] bool lockSelectedReference();
    [[nodiscard]] bool unlockSelectedReference();
    [[nodiscard]] bool removeLastConstraint();
    [[nodiscard]] bool removeConstraint(document::SketchConstraintId id);
    [[nodiscard]] bool removeSelected();
    [[nodiscard]] bool trimSelected();
    [[nodiscard]] bool extendSelected();
    [[nodiscard]] bool splitSelected();
    [[nodiscard]] bool filletSelected(double radius);
    [[nodiscard]] bool offsetSelected(double distance);
    [[nodiscard]] bool mirrorSelected();
    [[nodiscard]] bool linearPatternSelected(int count, double spacing);
    [[nodiscard]] bool circularPatternSelected(int count, double angleDegrees);
    // Records the current state as one undo step (no-op while a drag runs, U09).
    void commitHistory();
    // L05b: a recovered draft replaces the content as one undo step, solved as it was saved
    // (also under-determined or conflicting). An invalid draft changes nothing.
    [[nodiscard]] bool loadDraft(document::SketchElements elements,
                                 document::SketchConstraints constraints);
    // U09: a drag or a typed translation of the selection is one user action. dragSelected
    // moves from the state at beginDrag by the total offset (the candidate); endDrag records
    // one step, cancelDrag restores the starting state.
    void beginDrag();
    [[nodiscard]] bool dragSelected(double totalX, double totalY);
    void endDrag();
    void cancelDrag();
    [[nodiscard]] bool isDragging() const noexcept { return dragStart_.has_value(); }
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void setPolygonSides(int sides) noexcept;
    [[nodiscard]] std::optional<document::SketchElementId> selectedElement() const noexcept;
    [[nodiscard]] std::span<const document::SketchGeometryReference>
    selectedReferences() const noexcept;
    [[nodiscard]] bool isElementConflicting(document::SketchElementId elementId) const noexcept;
    // A click into empty space: nothing selected.
    void clearSelection() noexcept;
    // No degree of freedom left after the last successful solve.
    [[nodiscard]] bool isElementDetermined(document::SketchElementId elementId) const noexcept;
    [[nodiscard]] bool
    isConstraintConflicting(document::SketchConstraintId constraintId) const noexcept;
    // Linear dimensions/coordinates only; angles require the typed accessors.
    [[nodiscard]] std::optional<double>
    constraintValue(document::SketchConstraintId constraintId) const noexcept;
    [[nodiscard]] bool setConstraintValue(document::SketchConstraintId constraintId, double value);
    [[nodiscard]] std::optional<kernel::Radians>
    constraintAngle(document::SketchConstraintId constraintId) const noexcept;
    [[nodiscard]] bool setConstraintAngle(document::SketchConstraintId constraintId,
                                          kernel::Radians angle);
    [[nodiscard]] std::vector<features::SketchConstraintAnnotation> evaluatedConstraints() const;
    [[nodiscard]] std::span<const document::SketchElement> elements() const noexcept;
    [[nodiscard]] std::span<const document::SketchConstraint> constraints() const noexcept;
    [[nodiscard]] const features::SketchSolveResult& lastSolveResult() const noexcept;
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> evaluatedElements() const;
    [[nodiscard]] std::size_t closedProfileCount() const;
    // S02: a successful solve of nonempty geometry, also under-determined (open sketches are
    // valid); never a conflicting or failed solve, nor during a drag.
    [[nodiscard]] bool canAccept() const noexcept;
    [[nodiscard]] CommandResult tryAccept(ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::SketchParameters draftParameters() const { return parameters(false); }
    void cancel() noexcept;
    [[nodiscard]] bool isActive() const noexcept;

  private:
    [[nodiscard]] bool canAllocateIds(std::uint64_t elements,
                                      std::uint64_t constraints) const noexcept;
    [[nodiscard]] bool
    canAllocatePreviewIds(std::span<const document::SketchGeometry> preview) const noexcept;
    [[nodiscard]] bool canUpsertUnaryConstraint(document::SketchElementId element,
                                                document::SketchConstraintKind kind) const noexcept;
    [[nodiscard]] bool setConstraintInternalValue(document::SketchConstraintId constraintId,
                                                  double value);
    friend class SketchSolverGateTestAccess;
    // S02: everything an undo step, a drag or a failed operation restores. An undo step keeps
    // the selection with its geometry; its solve result is derived again when it returns.
    struct EditorState final {
        document::SketchElements elements;
        document::SketchConstraints constraints;
        std::optional<document::SketchElementId> selectedElement;
        std::vector<document::SketchGeometryReference> selectedReferences;
        std::optional<document::SketchPoint> selectionPoint;
        features::SketchSolveResult solve;
    };
    [[nodiscard]] EditorState snapshot() const;
    void restore(EditorState state) noexcept;
    // Puts the selection of `state` back where its elements still exist.
    void restoreSelection(const EditorState& state);
    // Restores the state from its construction unless keep() is called, so an operation that
    // fails or throws after a change leaves nothing half changed (S02).
    class Rollback final {
      public:
        explicit Rollback(SketchEditorSession& editor)
            : editor_(editor), saved_(editor.snapshot()) {}
        Rollback(const Rollback&) = delete;
        Rollback& operator=(const Rollback&) = delete;
        ~Rollback() {
            if (!kept_)
                editor_.restore(std::move(saved_));
        }
        void keep() noexcept { kept_ = true; }

      private:
        SketchEditorSession& editor_;
        EditorState saved_;
        bool kept_{false};
    };
    // S01: what the preview's items mean; points name an item by its position in preview_.
    struct PreviewPoint final {
        SketchCreationPoint role;
        std::size_t item;
        document::SketchPointPosition position;
    };
    struct PreviewShape final {
        std::vector<SketchCreationRole> roles;
        std::vector<PreviewPoint> points;
        std::optional<SketchArcDirection> arcDirection;
    };
    [[nodiscard]] SketchCreationResult
    confirmRectangle(const document::SketchRectangle& rectangle);
    void snapTo(document::SketchGeometryReference created, const std::optional<SketchSnap>& snap,
                document::SketchPoint point);
    [[nodiscard]] std::optional<SketchCreationResult> createStep(const SketchCreationInput& input);
    [[nodiscard]] std::optional<SketchCreationResult> createCentered(const SketchCreationInput& input);
    // Adds the constraints from `firstConstraint` on that the result does not list yet.
    void record(SketchCreationResult& result, std::uint64_t firstConstraint,
                SketchConstraintOrigin origin) const;
    [[nodiscard]] std::vector<document::SketchGeometry>
    geometry(document::SketchPoint first, document::SketchPoint second,
             std::optional<document::SketchPoint> third, PreviewShape& shape) const;
    [[nodiscard]] document::SketchParameters parameters(bool includePreview) const;
    // Solves the current state and records the result; on success the solved geometry is
    // kept. An exception or a changed support counts as failed, never as the last success.
    [[nodiscard]] bool solveConstraints();
    // The solve result of the current state without changing its geometry (undo, reopen).
    void refreshSolve();
    // Ends an operation that added geometry: keeps it if it is valid and solves.
    [[nodiscard]] bool keepIfValid(Rollback& rollback);
    // S08: transformed copies (mirror, pattern), SketchEditorOperations.cpp.
    struct Copy;
    [[nodiscard]] bool appendCopies(std::span<const Copy> copies);

    CadSession& session_;
    std::optional<document::FeatureId> supportFeature_;
    std::optional<document::DatumPlane> datum_;
    document::SketchPlane plane_;
    std::optional<document::SketchSupportBinding> supportBinding_;
    std::optional<DocumentStamp> supportStamp_;
    document::SketchElements elements_;
    document::SketchConstraints constraints_;
    std::optional<document::FeatureId> editingFeature_;
    // AI04c: the stored sketch when editing began; a change from outside (MCP) is not
    // overwritten silently. Empty for a sketch the session does not hold.
    std::optional<document::SketchParameters> editedBaseline_;
    [[nodiscard]] static std::optional<document::SketchParameters>
    storedSketch(const CadSession& session, document::FeatureId sketch);
    SketchDrawingTool tool_{SketchDrawingTool::line};
    std::vector<document::SketchGeometry> preview_;
    PreviewShape previewShape_;
    bool previewConfirmable_{false};
    std::optional<document::SketchElementId> selectedElement_;
    std::vector<document::SketchGeometryReference> selectedReferences_;
    std::optional<document::SketchPoint> selectionPoint_;
    features::SketchSolveResult lastSolveResult_{features::SketchSolveStatus::success, 0, {}};
    int polygonSides_{6};
    std::vector<EditorState> history_;
    std::size_t historyIndex_{0};
    std::optional<EditorState> dragStart_;
    bool active_{true};
};

} // namespace voelund::application
