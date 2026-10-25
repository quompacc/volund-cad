// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <functional>
#include <memory>

namespace voelund::viewport {

// T04c: push/pull in the viewport. A highlighted copy of the picked face carries an arrow
// along the face's outward normal; dragging the arrow moves the copy and reports the
// distance (snapped like the transform tool, Ctrl bypasses snapping). A typed distance
// moves the copy and the arrow the same way.
class PushPullHandle final {
  public:
    using Handler = std::function<void(double distance, bool finished)>;

    PushPullHandle();
    ~PushPullHandle();
    PushPullHandle(const PushPullHandle&) = delete;
    PushPullHandle& operator=(const PushPullHandle&) = delete;

    void show(const occ::handle<AIS_InteractiveContext>& context, const TopoDS_Shape& face,
              const gp_Dir& normal, const Quantity_Color& color);
    void hide();
    // The GL context was recreated: presentations are gone without removal.
    void reset();
    [[nodiscard]] bool isShown() const noexcept;
    void setDistance(double distance);
    // The way the arrow points: out of the body for a pull, into it for a push.
    [[nodiscard]] gp_Dir arrowDirection() const;
    // Where the arrow stands now (its base at the moved face), in model coordinates.
    [[nodiscard]] gp_Pnt gripPoint() const;
    [[nodiscard]] bool matches(const occ::handle<AIS_InteractiveObject>& object) const noexcept;
    [[nodiscard]] bool hasActiveMode() const noexcept;
    void start(int x, int y, const occ::handle<V3d_View>& view);
    void update(int x, int y, const occ::handle<V3d_View>& view, bool bypassSnapping);
    void finish();
    void setHandler(Handler handler);
    void setSnapping(bool enabled, double step);

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::viewport
