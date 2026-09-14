#pragma once
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace voelund::kernel {
namespace detail {
inline double finiteAngle(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("Angle must be finite");
    return value;
}
} // namespace detail

// Units are explicit; neither type converts implicitly to double or the other unit.
// Values are not wrapped: sign and complete turns remain part of the input.
class Degrees final {
  public:
    explicit Degrees(double value) : value_(detail::finiteAngle(value)) {}
    [[nodiscard]] double value() const noexcept { return value_; }

  private:
    double value_;
};
class Radians final {
  public:
    explicit Radians(double value) : value_(detail::finiteAngle(value)) {}
    [[nodiscard]] double value() const noexcept { return value_; }

  private:
    double value_;
};
[[nodiscard]] inline Radians toRadians(Degrees angle) {
    return Radians(angle.value() * (std::numbers::pi / 180.0));
}
[[nodiscard]] inline Degrees toDegrees(Radians angle) {
    return Degrees(angle.value() * (180.0 / std::numbers::pi));
}
} // namespace voelund::kernel
