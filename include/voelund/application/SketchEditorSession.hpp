#pragma once

#include <voelund/document/ModelDocument.hpp>
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
    [[nodiscard]] bool removeSelected();
    [[nodiscard]] std::optional<document::SketchElementId> selectedElement() const noexcept;
    [[nodiscard]] std::span<const document::SketchElement> elements() const noexcept;
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> evaluatedElements() const;
    [[nodiscard]] std::size_t closedProfileCount() const;
    [[nodiscard]] bool canAccept() const noexcept;
    [[nodiscard]] document::FeatureId accept();
    void cancel() noexcept;
    [[nodiscard]] bool isActive() const noexcept;

  private:
    [[nodiscard]] document::SketchGeometry geometry(document::SketchPoint first,
                                                    document::SketchPoint second) const;
    [[nodiscard]] document::SketchParameters parameters(bool includePreview) const;

    CadSession& session_;
    document::FeatureId supportFeature_;
    document::SketchPlane plane_;
    document::SketchElements elements_;
    std::optional<document::FeatureId> editingFeature_;
    SketchDrawingTool tool_{SketchDrawingTool::line};
    std::optional<document::SketchGeometry> preview_;
    std::optional<document::SketchElementId> selectedElement_;
    bool active_{true};
};

} // namespace voelund::application
