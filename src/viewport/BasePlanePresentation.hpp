#pragma once
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>

#include <array>
#include <optional>

namespace voelund::viewport {
// V01b (user decision 2026-10-04): XY/XZ/YZ as selectable translucent squares through the
// origin. Prominent in an empty scene, faint beside bodies (still pickable, hover reveals
// them), hidden while a sketch is being drawn.
class BasePlanePresentation final {
  public:
    void show(const occ::handle<AIS_InteractiveContext>& context, bool emphasized, double halfSize);
    void hide(const occ::handle<AIS_InteractiveContext>& context);
    [[nodiscard]] std::optional<BasePlane>
    plane(const occ::handle<AIS_InteractiveObject>& object) const;
    [[nodiscard]] std::size_t displayedCount() const noexcept;

  private:
    std::array<occ::handle<AIS_Shape>, 3> planes_;
};
} // namespace voelund::viewport
