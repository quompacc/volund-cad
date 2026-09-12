#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SketchConstraintAnnotations.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <optional>
#include <span>
#include <vector>

namespace voelund::application {

class CadSession;

enum class SketchDrawingTool { line, rectangle, circle };

class SketchEditorSession final {
  public:
    SketchEditorSession(CadSession& session, document::FeatureId supportFeature,
                        document::SketchPlane plane);
    SketchEditorSession(CadSession& session, document::FeatureId sketchFeature,
                        document::FeatureId supportFeature, document::SketchParameters parameters);

    void setTool(SketchDrawingTool tool) noexcept;
    [[nodiscard]] SketchDrawingTool tool() const noexcept;
    [[nodiscard]] bool setPreview(document::SketchPoint first, document::SketchPoint second);
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
    [[nodiscard]] bool isSelectedConstruction() const noexcept;
    [[nodiscard]] bool toggleSelectedConstruction();
    [[nodiscard]] bool canLockSelectedReference() const noexcept;
    [[nodiscard]] bool isSelectedReferenceLocked() const noexcept;
    [[nodiscard]] bool lockSelectedReference();
    [[nodiscard]] bool unlockSelectedReference();
    [[nodiscard]] bool removeLastConstraint();
    [[nodiscard]] bool removeSelected();
    [[nodiscard]] std::optional<document::SketchElementId> selectedElement() const noexcept;
    [[nodiscard]] std::span<const document::SketchGeometryReference>
    selectedReferences() const noexcept;
    [[nodiscard]] bool isElementConflicting(document::SketchElementId elementId) const noexcept;
    [[nodiscard]] bool
    isConstraintConflicting(document::SketchConstraintId constraintId) const noexcept;
    [[nodiscard]] std::optional<double>
    constraintValue(document::SketchConstraintId constraintId) const noexcept;
    [[nodiscard]] bool setConstraintValue(document::SketchConstraintId constraintId, double value);
    [[nodiscard]] std::vector<features::SketchConstraintAnnotation> evaluatedConstraints() const;
    [[nodiscard]] std::span<const document::SketchElement> elements() const noexcept;
    [[nodiscard]] std::span<const document::SketchConstraint> constraints() const noexcept;
    [[nodiscard]] const features::SketchSolveResult& lastSolveResult() const noexcept;
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> evaluatedElements() const;
    [[nodiscard]] std::size_t closedProfileCount() const;
    [[nodiscard]] bool canAccept() const noexcept;
    [[nodiscard]] document::FeatureId accept();
    void cancel() noexcept;
    [[nodiscard]] bool isActive() const noexcept;

  private:
    [[nodiscard]] document::SketchElementId
    confirmRectangle(const document::SketchRectangle& rectangle);
    [[nodiscard]] document::SketchGeometry geometry(document::SketchPoint first,
                                                    document::SketchPoint second) const;
    [[nodiscard]] document::SketchParameters parameters(bool includePreview) const;
    [[nodiscard]] bool solveConstraints();

    CadSession& session_;
    document::FeatureId supportFeature_;
    document::SketchPlane plane_;
    document::SketchElements elements_;
    document::SketchConstraints constraints_;
    std::optional<document::FeatureId> editingFeature_;
    SketchDrawingTool tool_{SketchDrawingTool::line};
    std::optional<document::SketchGeometry> preview_;
    std::optional<document::SketchElementId> selectedElement_;
    std::vector<document::SketchGeometryReference> selectedReferences_;
    features::SketchSolveResult lastSolveResult_{features::SketchSolveStatus::success, -1, {}};
    bool active_{true};
};

} // namespace voelund::application
