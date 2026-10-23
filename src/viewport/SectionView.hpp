// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include "PushPullHandle.hpp"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <Quantity_Color.hxx>

#include <functional>
#include <optional>

namespace voelund::viewport {

// N05: the section view. One clipping plane on every body presentation (bodies only: sketches,
// planes and grips stay whole); the cut is filled in a darker shade of the body colour. A
// transparent patch on the plane carries an arrow along its normal; dragging it moves the
// plane and reports the moved plane. Selection follows the clipping, so a hidden part can't
// be picked.
class SectionView final {
  public:
    using Handler = std::function<void(const SectionPlane& plane, bool finished)>;

    // Shows, moves or (nullopt) removes the section. A plane only shifted along the shown
    // normal moves the patch; any other plane shows it anew, sized from `extent`.
    void set(const occ::handle<AIS_InteractiveContext>& context,
             const std::optional<SectionPlane>& plane, const Bnd_Box& extent,
             const Quantity_Color& bodyColor, const Quantity_Color& gripColor);
    // Adds the clipping plane to a body presentation (off while no section is shown).
    void clip(const occ::handle<AIS_Shape>& body);
    // The GL context was recreated: presentations are gone without removal.
    void reset();
    [[nodiscard]] bool isShown() const noexcept { return plane_.has_value(); }
    [[nodiscard]] const std::optional<SectionPlane>& plane() const noexcept { return plane_; }
    [[nodiscard]] PushPullHandle& grip() noexcept { return grip_; }
    void setHandler(Handler handler);

  private:
    void showGrip(const occ::handle<AIS_InteractiveContext>& context, const Bnd_Box& extent,
                  const Quantity_Color& gripColor);
    void moved(double distance, bool finished);
    [[nodiscard]] SectionPlane shifted(double distance) const;

    occ::handle<Graphic3d_ClipPlane> clipPlane_;
    PushPullHandle grip_;
    std::optional<SectionPlane> plane_;
    // Where the grip was shown; its distance counts from here against the normal.
    SectionPlane base_{};
    Handler handler_;
};

} // namespace voelund::viewport
