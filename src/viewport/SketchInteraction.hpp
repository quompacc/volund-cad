#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>

#include <optional>

namespace voelund::viewport {

[[nodiscard]] std::optional<PlanarFaceSelection> planarFaceSelection(
    std::uint64_t bodyId,
    const TopoDS_Shape& detectedShape);
[[nodiscard]] std::optional<PlanarFaceSelection>
resolvePlanarFaceSelection(const PlanarFaceSelection& previous, const TopoDS_Shape& supportShape);

class SketchInteraction final {
public:
    void begin(
        const PlanarFaceSelection& support,
        const occ::handle<AIS_InteractiveContext>& context,
        const occ::handle<V3d_View>& view);
    void cancel();
    [[nodiscard]] bool isActive() const noexcept;
    void setSnapping(bool enabled, double linearStep);
    void setDistancePreviewHandler(std::function<void(double)> handler);
    void updateDistance(double distance);
    void move(int x, int y);
    [[nodiscard]] std::optional<SketchRectangleResult> click(int x, int y);

private:
    [[nodiscard]] std::optional<std::pair<double, double>> project(int x, int y) const;
    void showRectanglePreview(double x, double y);
    void showExtrusionPreview();

    PlanarFaceSelection support_{};
    occ::handle<AIS_InteractiveContext> context_;
    occ::handle<V3d_View> view_;
    occ::handle<AIS_Shape> preview_;
    std::optional<std::pair<double, double>> firstPoint_;
    std::optional<std::pair<double, double>> secondPoint_;
    std::function<void(double)> distancePreviewHandler_;
    int extrusionAnchorY_{0};
    double distance_{1.0};
    double linearStep_{1.0};
    bool snapping_{true};
    bool active_{false};
};

} // namespace voelund::viewport
