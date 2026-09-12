#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <V3d_View.hxx>

#include <optional>
#include <utility>

namespace voelund::viewport {

class SketchEditorInteraction final {
public:
    void begin(const PlanarFaceSelection& support, SketchElementTool tool,
               const occ::handle<V3d_View>& view);
    void cancel() noexcept;
    void setTool(SketchElementTool tool) noexcept;
    void setSnapping(bool enabled, double linearStep) noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] std::optional<SketchElementIntent> move(int x, int y) const;
    [[nodiscard]] std::optional<SketchElementIntent> click(int x, int y);

private:
    [[nodiscard]] std::optional<std::pair<double, double>> project(int x, int y) const;
    [[nodiscard]] bool valid(const SketchElementIntent& intent) const noexcept;

    PlanarFaceSelection support_{};
    SketchElementTool tool_{SketchElementTool::line};
    occ::handle<V3d_View> view_;
    std::optional<std::pair<double, double>> firstPoint_;
    double linearStep_{1.0};
    bool snapping_{true};
    bool active_{false};
};

} // namespace voelund::viewport
