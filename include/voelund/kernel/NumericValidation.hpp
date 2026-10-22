// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace voelund::kernel {
[[nodiscard]] inline bool finiteValues(std::initializer_list<double> values) noexcept {
    for (double value : values)
        if (!std::isfinite(value))
            return false;
    return true;
}
[[nodiscard]] inline bool positiveFinite(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

// No OCCT objects or mutable global tolerance. Normalize before products/squares.
class UnitDirection final {
  public:
    [[nodiscard]] static UnitDirection fromComponents(double x, double y, double z,
                                                      double minimumMagnitude = 0.0) {
        if (!finiteValues({x, y, z, minimumMagnitude}) || minimumMagnitude < 0.0)
            throw std::invalid_argument("Direction components and magnitude limit must be finite");
        // Do not call three-argument hypot on unscaled values: library
        // implementations differ at extreme magnitudes.
        const double scale = std::max({std::abs(x), std::abs(y), std::abs(z)});
        if (scale == 0.0)
            throw std::invalid_argument("Direction magnitude must not be zero");
        const double sx = x / scale, sy = y / scale, sz = z / scale;
        const double scaledMagnitude = std::hypot(sx, sy, sz);
        if (scale > std::numeric_limits<double>::max() / scaledMagnitude ||
            scale <= minimumMagnitude / scaledMagnitude)
            throw std::invalid_argument(
                "Direction magnitude must be finite and exceed its minimum");
        return UnitDirection(sx / scaledMagnitude, sy / scaledMagnitude, sz / scaledMagnitude);
    }
    [[nodiscard]] double x() const noexcept { return x_; }
    [[nodiscard]] double y() const noexcept { return y_; }
    [[nodiscard]] double z() const noexcept { return z_; }
    [[nodiscard]] double dot(const UnitDirection& other) const noexcept {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

  private:
    UnitDirection(double x, double y, double z) : x_(x), y_(y), z_(z) {}
    double x_;
    double y_;
    double z_;
};
} // namespace voelund::kernel
