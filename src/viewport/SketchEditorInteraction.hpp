#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <V3d_View.hxx>

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace voelund::viewport {

class SketchEditorInteraction final {
  public:
    // S04a: what the cursor would snap to, in framebuffer pixels, for the snap markers.
    struct SnapFeedback final {
        std::vector<std::pair<int, int>> candidates;
        std::optional<std::pair<int, int>> active;
        std::optional<SketchPointKind> activeKind;
        std::optional<SketchAutomaticConstraint> relation;
    };

    void begin(const PlanarFaceSelection& support, SketchElementTool tool,
               const occ::handle<V3d_View>& view);
    void cancel() noexcept;
    void rebindView(const occ::handle<V3d_View>& view) { view_ = view; }
    void setTool(SketchElementTool tool) noexcept;
    void continuePolyline(double x, double y, std::uint64_t elementId) noexcept;
    void setSnapping(bool enabled, double linearStep) noexcept;
    void setSnapPoints(std::span<const SceneSketchPoint> points);
    // Ctrl held: no point, axis or object snapping for this pointer position.
    void setSnapBypass(bool bypass) noexcept { bypass_ = bypass; }
    // S04: off, a step still lands on the snapped points but binds none of them and suggests
    // no relation; a continued polyline stays joined to its previous segment.
    void setAutomaticRelations(bool enabled) noexcept { automaticRelations_ = enabled; }
    // S13: hidden projected geometry is not snapped to either.
    void setProjectedVisible(bool visible) noexcept { projectedVisible_ = visible; }
    [[nodiscard]] bool projectedVisible() const noexcept { return projectedVisible_; }
    [[nodiscard]] SnapFeedback snapFeedback(int x, int y) const;
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
    [[nodiscard]] std::pair<std::pair<double, double>, std::optional<SketchAutomaticConstraint>>
    relatedLineEnd(std::pair<double, double> first, std::pair<double, double> second) const;
    [[nodiscard]] bool
    tangentToReferencedLine(std::pair<double, double> center, std::pair<double, double> radialPoint,
                            const std::optional<SketchPointReferenceIntent>& reference) const;
    [[nodiscard]] std::tuple<std::pair<double, double>, std::optional<SketchAutomaticConstraint>,
                             std::optional<std::uint64_t>>
    relatedRadialPoint(std::pair<double, double> center,
                       std::pair<double, double> radialPoint) const;
    [[nodiscard]] bool
    isRadialCenterReference(const std::optional<SketchPointReferenceIntent>& reference) const;
    [[nodiscard]] SketchElementIntent related(SketchElementIntent intent) const noexcept {
        if (automaticRelations_)
            return intent;
        intent.automaticConstraint.reset();
        intent.automaticTargetId.reset();
        const bool continued = tool_ == SketchElementTool::polyline && continued_.has_value() &&
                               intent.firstReference.has_value() &&
                               intent.firstReference->elementId == *continued_;
        if (!continued)
            intent.firstReference.reset();
        intent.secondReference.reset();
        intent.thirdReference.reset();
        return intent;
    }

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
    bool bypass_{false};
    bool active_{false};
    bool automaticRelations_{true};
    bool projectedVisible_{true};
    std::optional<std::uint64_t> continued_; // the polyline segment the next one continues
};

} // namespace voelund::viewport
