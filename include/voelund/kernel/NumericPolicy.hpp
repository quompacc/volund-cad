// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <Precision.hxx>
#include <algorithm>
#include <cmath>

namespace voelund::kernel {
// Millimetres. These are input limits, not a promise that every Boolean succeeds.
inline constexpr double minimumModelLength = 0.001;
inline constexpr double maximumModelLength = 1000000.0;
inline constexpr const char* modelRangeDiagnostic =
    "length must be in [0.001, 1000000] mm; coordinates in [-1000000, 1000000] mm";

[[nodiscard]] inline bool modelLength(double value) noexcept {
    return std::isfinite(value) && value >= minimumModelLength && value <= maximumModelLength;
}
[[nodiscard]] inline bool modelCoordinate(double value) noexcept {
    return std::isfinite(value) && std::abs(value) <= maximumModelLength;
}

struct ConnectionTolerance final {
    double millimetres;
};
struct AngularTolerance final {
    double radians;
};
struct ModelComparisonTolerance final {
    double relative;
};
struct PickingTolerance final {
    int pixels;
};

// The floor comes from the OCCT headers used by this build, never a copied constant.
// Relative allowance: one part in 10^12 of the local coordinate magnitude.
[[nodiscard]] inline ConnectionTolerance connectionTolerance(double scale) noexcept {
    return {std::max(Precision::Confusion(), std::abs(scale) * 1.0e-12)};
}
inline constexpr AngularTolerance angularTolerance{1.0e-9};
inline constexpr ModelComparisonTolerance modelComparisonTolerance{1.0e-10};

} // namespace voelund::kernel
