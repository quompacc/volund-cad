#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_TextLabel.hxx>
#include <Quantity_Color.hxx>
#include <V3d_View.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <span>
#include <vector>

namespace voelund::viewport {

class SketchConstraintPresentation final {
  public:
    void setConstraints(const occ::handle<AIS_InteractiveContext>& context,
                        std::span<const SceneSketchConstraint> constraints);
    void clear(const occ::handle<AIS_InteractiveContext>& context);
    // U11: the regular label color of the theme (dark on light, light on dark).
    void setLabelColor(const Quantity_Color& color);
    [[nodiscard]] std::size_t size() const noexcept;
    // U11: the label under a framebuffer pixel. Labels are drawn a few pixels beside their
    // anchor; OCCT picking of such pixel-frame text is unreliable, so the hit test compares
    // the pixel with the projected text box itself.
    [[nodiscard]] std::optional<std::uint64_t> constraintAt(const occ::handle<V3d_View>& view,
                                                            int x, int y) const;

  private:
    struct ConstraintPresentation final {
        std::uint64_t id;
        occ::handle<AIS_TextLabel> label;
        bool editable;
        gp_Pnt anchor;
        double offsetX;
        double offsetY;
        double height;
        int characters;
    };

    std::vector<ConstraintPresentation> constraints_;
    Quantity_Color labelColor_{0.04, 0.31, 0.61, Quantity_TOC_RGB};
};

} // namespace voelund::viewport
