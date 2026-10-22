// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <AIS_InteractiveObject.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace voelund::viewport {

class TransformManipulator final {
  public:
    using MoveHandler = std::function<void(std::uint64_t, double, double, double)>;
    using RotationHandler = std::function<void(std::uint64_t, int, double)>;

    TransformManipulator();
    ~TransformManipulator();

    TransformManipulator(const TransformManipulator&) = delete;
    TransformManipulator& operator=(const TransformManipulator&) = delete;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept;
    void attach(const occ::handle<AIS_Shape>& presentation, std::uint64_t bodyId);
    // N02b: the target first, then the bodies that move with it; a group only moves.
    void attach(const std::vector<occ::handle<AIS_Shape>>& presentations, std::uint64_t bodyId);
    void detach();
    void resetPresentation();
    [[nodiscard]] bool
    matches(const occ::handle<AIS_InteractiveObject>& interactiveObject) const noexcept;
    [[nodiscard]] bool hasActiveMode() const noexcept;
    void start(int x, int y, const occ::handle<V3d_View>& view);
    void update(int x, int y, const occ::handle<V3d_View>& view, bool bypassSnapping);
    void finish();
    // S14 (A-TXN-03): ends the drag without a move; the preview returns to the start.
    void cancel();

    void setMoveHandler(MoveHandler handler);
    void setMovePreviewHandler(MoveHandler handler);
    void setRotationHandler(RotationHandler handler);
    void setRotationPreviewHandler(RotationHandler handler);
    void setSnapping(bool enabled, double linearStep, double angularStep);

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::viewport
