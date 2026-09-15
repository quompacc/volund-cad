#pragma once
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <stdexcept>
#include <type_traits>

namespace voelund::document::detail {
inline void validateGeometryRange(const SketchGeometry& geometry) {
    const auto point = [](const SketchPoint& p) {
        return kernel::modelCoordinate(p.x) && kernel::modelCoordinate(p.y);
    };
    const bool valid = std::visit([&](const auto& shape) {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, SketchLine>) {
            return point(shape.start) && point(shape.end) && kernel::modelLength(
                std::hypot(shape.end.x - shape.start.x, shape.end.y - shape.start.y));
        } else if constexpr (std::is_same_v<T, SketchRectangle>) {
            return point(shape.first) && point(shape.second) &&
                   kernel::modelLength(std::abs(shape.second.x - shape.first.x)) &&
                   kernel::modelLength(std::abs(shape.second.y - shape.first.y));
        } else {
            const double radius = [&] {
                if constexpr (std::is_same_v<T, SketchEllipse>) return shape.majorRadius;
                else return shape.radius;
            }();
            bool radii = kernel::modelLength(radius);
            if constexpr (std::is_same_v<T, SketchEllipse>)
                radii = radii && kernel::modelLength(shape.minorRadius);
            if constexpr (std::is_same_v<T, SketchArc>)
                radii = radii && kernel::modelLength(radius * (shape.endAngle - shape.startAngle));
            return point(shape.center) && radii &&
                   point({shape.center.x - radius, shape.center.y - radius}) &&
                   point({shape.center.x + radius, shape.center.y + radius});
        }
    }, geometry);
    if (!valid) throw std::invalid_argument(kernel::modelRangeDiagnostic);
}
} // namespace voelund::document::detail
