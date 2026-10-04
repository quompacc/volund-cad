#pragma once
#include <voelund/application/CommandTransaction.hpp>
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
    void commitHistory();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void setPolygonSides(int sides) noexcept;
    [[nodiscard]] std::optional<document::SketchElementId> selectedElement() const noexcept;
    [[nodiscard]] std::span<const document::SketchGeometryReference>
    selectedReferences() const noexcept;
    [[nodiscard]] bool isElementConflicting(document::SketchElementId elementId) const noexcept;
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
    struct HistoryState final {
        document::SketchElements elements;
        document::SketchConstraints constraints;
    };
    [[nodiscard]] document::SketchElementId
    confirmRectangle(const document::SketchRectangle& rectangle);
    [[nodiscard]] std::vector<document::SketchGeometry>
    geometry(document::SketchPoint first, document::SketchPoint second,
             std::optional<document::SketchPoint> third) const;
    [[nodiscard]] document::SketchParameters parameters(bool includePreview) const;
    [[nodiscard]] bool solveConstraints();

    CadSession& session_;
    std::optional<document::FeatureId> supportFeature_;
    std::optional<document::DatumPlane> datum_;
    document::SketchPlane plane_;
    std::optional<document::SketchSupportBinding> supportBinding_;
    std::optional<DocumentStamp> supportStamp_;
    document::SketchElements elements_;
    document::SketchConstraints constraints_;
    std::optional<document::FeatureId> editingFeature_;
    SketchDrawingTool tool_{SketchDrawingTool::line};
    std::vector<document::SketchGeometry> preview_;
    bool previewConfirmable_{false};
    std::optional<document::SketchElementId> selectedElement_;
    std::vector<document::SketchGeometryReference> selectedReferences_;
    std::optional<document::SketchPoint> selectionPoint_;
    features::SketchSolveResult lastSolveResult_{features::SketchSolveStatus::success, -1, {}};
    int polygonSides_{6};
    std::vector<HistoryState> history_;
    std::size_t historyIndex_{0};
    bool active_{true};
};

} // namespace voelund::application
