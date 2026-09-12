#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_Shape.hxx>

#include <optional>
#include <span>
#include <vector>

namespace voelund::viewport {

class SketchPresentation final {
  public:
    void setElements(const occ::handle<AIS_InteractiveContext>& context,
                     std::span<const SceneSketchElement> elements,
                     std::span<const std::uint64_t> selectedIds);
    void clear(const occ::handle<AIS_InteractiveContext>& context);
    void select(const occ::handle<AIS_InteractiveContext>& context,
                std::span<const std::uint64_t> selectedIds);
    [[nodiscard]] std::optional<std::uint64_t>
    detectedElement(const occ::handle<AIS_InteractiveObject>& detected) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    struct ElementPresentation final {
        std::uint64_t id;
        occ::handle<AIS_Shape> shape;
        bool conflicting;
    };

    std::vector<ElementPresentation> elements_;
};

} // namespace voelund::viewport
