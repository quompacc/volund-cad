// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_TextLabel.hxx>
#include <Quantity_Color.hxx>
#include <V3d_View.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace voelund::viewport {

class SketchGlyphOverlay;

class SketchConstraintPresentation final {
  public:
    // W18: relations with symbol anchors are drawn there, not as OCCT text.
    void setGlyphOverlay(SketchGlyphOverlay* overlay) noexcept { glyphs_ = overlay; }
    void setConstraints(const occ::handle<AIS_InteractiveContext>& context,
                        std::span<const SceneSketchConstraint> constraints);
    void clear(const occ::handle<AIS_InteractiveContext>& context);
    // U11: the regular label color of the theme (dark on light, light on dark).
    void setLabelColor(const Quantity_Color& color);
    // User report 2026-10-25: the screen's scale (2 on a 200 % screen). Text heights follow
    // the view's resolution; offsets, arrows and hit boxes in framebuffer pixels follow this.
    void setPixelScale(double scale) noexcept { pixelScale_ = scale; }
    [[nodiscard]] double pixelScale() const noexcept { return pixelScale_; }
    [[nodiscard]] std::size_t size() const noexcept;
    // U11: the label under a framebuffer pixel. Labels are drawn a few pixels beside their
    // anchor; OCCT picking of such pixel-frame text is unreliable, so the hit test compares
    // the pixel with the projected text box itself.
    // W18: the text boxes of the shown labels (framebuffer pixels); relation symbols avoid them.
    [[nodiscard]] std::vector<std::array<double, 4>>
    labelBoxes(const occ::handle<V3d_View>& view) const;
    [[nodiscard]] std::optional<std::uint64_t> constraintAt(const occ::handle<V3d_View>& view,
                                                            int x, int y) const;

  private:
    struct Placement final {
        gp_Pnt anchor;
        double offsetX;
        double offsetY;
        occ::handle<AIS_InteractiveObject> drawing;
    };
    [[nodiscard]] Placement placeDimension(const occ::handle<AIS_InteractiveContext>& context,
                                           const SceneDimension& dimension,
                                           const Quantity_Color& color, double height,
                                           int characters) const;
    [[nodiscard]] Quantity_Color colorOf(const SceneSketchConstraint& constraint) const;
    [[nodiscard]] bool asSymbol(const SceneSketchConstraint& constraint) const;
    struct ConstraintPresentation final {
        std::uint64_t id;
        occ::handle<AIS_TextLabel> label;
        occ::handle<AIS_InteractiveObject> drawing; // W12: lines and arrows of a dimension
        bool editable;
        gp_Pnt anchor;
        double offsetX;
        double offsetY;
        double height;
        int characters;
    };

    std::vector<ConstraintPresentation> constraints_;
    Quantity_Color labelColor_{0.04, 0.31, 0.61, Quantity_TOC_RGB};
    double pixelScale_{1.0};
    SketchGlyphOverlay* glyphs_{nullptr};
    std::size_t glyphCount_{0};
};

} // namespace voelund::viewport
