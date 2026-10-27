// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OperationHistory.hpp>

#include <array>

namespace voelund::kernel {

// N09: the solid a hole removes, as a profile turned about the hole's axis from the entrance
// (`origin`) along `direction` into the body. Its faces carry roles: the entrance
// cylinderStart, the wall cylinderSide, a flat bottom cylinderEnd, and holeCounterboreSide /
// holeCounterboreFloor, holeCountersink or holeTip for the extras. Lengths in mm, angles in
// radians (full cone angles).
struct HoleProfile final {
    double radius;
    double length;          // entrance to the shoulder (the start of a drill point)
    double headRadius{0.0}; // counterbore or countersink at the entrance; 0: none
    double headDepth{0.0};  // counterbore depth; 0 with a head radius: a countersink
    double countersinkAngle{0.0};
    double tipAngle{0.0}; // drill point; 0: a flat bottom
    std::array<double, 3> origin{};
    std::array<double, 3> direction{0.0, 0.0, 1.0};
};
[[nodiscard]] ShapeOperationResult makeHoleToolWithHistory(const HoleProfile& profile);

} // namespace voelund::kernel
