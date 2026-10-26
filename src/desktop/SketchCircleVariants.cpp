// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchCircleVariants.hpp"

#include <cmath>

namespace voelund::desktop {

std::optional<viewport::SketchElementIntent>
circleThrough(const viewport::SketchElementIntent& clicks, bool threePoints) {
    const double ax = clicks.firstX, ay = clicks.firstY;
    const double bx = clicks.secondX, by = clicks.secondY;
    double cx = (ax + bx) / 2, cy = (ay + by) / 2;
    if (threePoints) {
        if (!clicks.third)
            return std::nullopt;
        const auto [px, py] = *clicks.third;
        // The circumcentre of the three points.
        const double d = 2 * (ax * (by - py) + bx * (py - ay) + px * (ay - by));
        if (std::abs(d) < 1e-12)
            return std::nullopt;
        const double a2 = ax * ax + ay * ay, b2 = bx * bx + by * by, p2 = px * px + py * py;
        cx = (a2 * (by - py) + b2 * (py - ay) + p2 * (ay - by)) / d;
        cy = (a2 * (px - bx) + b2 * (ax - px) + p2 * (bx - ax)) / d;
    }
    if (std::hypot(bx - cx, by - cy) < 1e-9)
        return std::nullopt;
    viewport::SketchElementIntent circle{};
    circle.firstX = cx;
    circle.firstY = cy;
    circle.secondX = bx;
    circle.secondY = by;
    return circle;
}

} // namespace voelund::desktop
