#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <V3d_View.hxx>

#include <optional>
#include <tuple>
#include <utility>

namespace voelund::viewport {

class SketchEditorInteraction final {
  public:
    void begin(const PlanarFaceSelection& support, SketchElementTool tool,
               const occ::handle<V3d_View>& view);
    void cancel() noexcept;
    void rebindView(const occ::handle<V3d_View>& view) { view_ = view; }
    void setTool(SketchElementTool tool) noexcept;
    void continuePolyline(double x, double y, std::uint64_t elementId) noexcept;
    void setSnapping(bool enabled, double linearStep) noexcept;
    void setSnapPoints(std::span<const SceneSketchPoint> points);
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isDrawing() const noexcept;
    [[nodiscard]] std::optional<std::pair<double, double>> projectPoint(int x, int y) const;
    [[nodiscard]] std::optional<SketchElementIntent> move(int x, int y) const;
    [[nodiscard]] std::optional<SketchElementIntent> click(int x, int y);
    [[nodiscard]] bool beginElementDrag(int x, int y);
    [[nodiscard]] std::optional<std::pair<double, double>> dragElement(int x, int y);
    void finishElementDrag() noexcept;
    [[nodiscard]] bool cancelStep() noexcept;
    [[nodiscard]] bool isElementDragging() const noexcept;

  private:
    struct InteractionPoint final {
        std::pair<double, double> position;
        std::optional<SketchPointReferenceIntent> reference;
    };

    [[nodiscard]] bool valid(const SketchElementIntent& intent) const noexcept;
    [[nodiscard]] std::optional<InteractionPoint> interactionPoint(int x, int y) const;
    [[nodiscard]] std::pair<std::pair<double, double>,
                            std::optional<SketchAutomaticConstraint>>
    relatedLineEnd(std::pair<double, double> first, std::pair<double, double> second) const;
    [[nodiscard]] bool tangentToReferencedLine(
        std::pair<double, double> center, std::pair<double, double> radialPoint,
        const std::optional<SketchPointReferenceIntent>& reference) const;
    [[nodiscard]] std::tuple<std::pair<double, double>,
                             std::optional<SketchAutomaticConstraint>,
                             std::optional<std::uint64_t>>
    relatedRadialPoint(std::pair<double, double> center,
                       std::pair<double, double> radialPoint) const;
    [[nodiscard]] bool isRadialCenterReference(
        const std::optional<SketchPointReferenceIntent>& reference) const;

    PlanarFaceSelection support_{};
    SketchElementTool tool_{SketchElementTool::line};
    occ::handle<V3d_View> view_;
    std::optional<std::pair<double, double>> firstPoint_;
    std::optional<std::pair<double, double>> secondPoint_;
    std::optional<SketchPointReferenceIntent> firstReference_;
    std::optional<SketchPointReferenceIntent> secondReference_;
    std::vector<SceneSketchPoint> snapPoints_;
    std::optional<std::pair<double, double>> dragPoint_;
    double linearStep_{1.0};
    bool snapping_{true};
    bool active_{false};
};

} // namespace voelund::viewport
