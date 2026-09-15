#pragma once
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace voelund::features::detail {
[[nodiscard]] inline kernel::ConnectionTolerance
connectionTolerance(const document::SketchPoint& first, const document::SketchPoint& second) {
    return kernel::connectionTolerance(
        std::max({std::abs(first.x), std::abs(first.y), std::abs(second.x), std::abs(second.y)}));
}
[[nodiscard]] inline bool connected(const document::SketchPoint& first,
                                    const document::SketchPoint& second) {
    return std::hypot(second.x - first.x, second.y - first.y) <=
           connectionTolerance(first, second).millimetres;
}
} // namespace voelund::features::detail
