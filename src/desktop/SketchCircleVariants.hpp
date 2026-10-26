// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <optional>

namespace voelund::desktop {

// W20 (as SolidCraft's circle family): the circle a two-point or three-point click makes, as the
// circle tool's intent (centre, point on the circle). Two points are the ends of a diameter;
// three points lie on the circle. None when three points lie on one line. Snaps, suggested
// relations and typed values of the line or arc clicks do not carry over.
[[nodiscard]] std::optional<viewport::SketchElementIntent>
circleThrough(const viewport::SketchElementIntent& clicks, bool threePoints);

} // namespace voelund::desktop
