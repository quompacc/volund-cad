#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <Quantity_Color.hxx>

#include <optional>
#include <span>
#include <vector>

namespace voelund::viewport {

class SketchPresentation final {
  public:
    void setElements(const occ::handle<AIS_InteractiveContext>& context,
                     std::span<const SceneSketchElement> elements,
                     std::span<const std::uint64_t> selectedIds, bool showPoints);
    void clear(const occ::handle<AIS_InteractiveContext>& context);
    void setReferences(const occ::handle<AIS_InteractiveContext>& context,
                       const PlanarFaceSelection& support, bool visible);
    void select(const occ::handle<AIS_InteractiveContext>& context,
                std::span<const std::uint64_t> selectedIds);
    [[nodiscard]] std::optional<std::uint64_t>
    detectedElement(const occ::handle<AIS_InteractiveObject>& detected) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    // Colors of free and fully determined geometry (theme); call select() to recolor.
    void setColors(const Quantity_Color& free, const Quantity_Color& determined);
    // For tests: whether the element is drawn dashed, and its color.
    [[nodiscard]] std::optional<bool> dashed(std::uint64_t id) const;
    [[nodiscard]] std::optional<Quantity_Color> color(std::uint64_t id) const;

  private:
    struct ElementPresentation final {
        std::uint64_t id;
        occ::handle<AIS_Shape> shape;
        std::vector<occ::handle<AIS_Point>> points;
        bool conflicting;
        bool construction;
        bool determined;
    };

    std::vector<ElementPresentation> elements_;
    std::vector<occ::handle<AIS_Shape>> referenceAxes_;
    std::vector<occ::handle<AIS_Shape>> projectedEdges_;
    occ::handle<AIS_Point> origin_;
    Quantity_Color free_{0.12, 0.44, 0.92, Quantity_TOC_RGB};
    Quantity_Color determined_{0.11, 0.13, 0.15, Quantity_TOC_RGB};
};

} // namespace voelund::viewport
