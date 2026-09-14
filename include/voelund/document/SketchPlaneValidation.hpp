#pragma once
#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/NumericValidation.hpp>

namespace voelund::document {
// D10a preserves legacy thresholds and stored axis magnitudes. These are
// dimensionless direction checks, not distances, connection or picking tolerances.
inline void validateSketchPlane(const SketchPlane& plane) {
    if (!kernel::finiteValues({plane.originX, plane.originY, plane.originZ}))
        throw std::invalid_argument("Sketch plane origin must contain finite values");
    constexpr double minimumDirectionMagnitude = 1.0e-9;
    constexpr double maximumPerpendicularDot = 1.0e-8;
    const auto normal = kernel::UnitDirection::fromComponents(
        plane.normalX, plane.normalY, plane.normalZ, minimumDirectionMagnitude);
    const auto x = kernel::UnitDirection::fromComponents(
        plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ, minimumDirectionMagnitude);
    if (std::abs(normal.dot(x)) > maximumPerpendicularDot)
        throw std::invalid_argument("Sketch plane directions must be perpendicular");
}
// Product inputs must already be orthonormal. Do not normalize persisted
// legacy axes: their magnitude used to change extrusion and support projection.
inline void validateCanonicalSketchPlane(const SketchPlane& plane) {
    validateSketchPlane(plane);
    constexpr double basisTolerance = 64.0 * std::numeric_limits<double>::epsilon();
    const auto unit = [](double x, double y, double z) {
        constexpr double tolerance = 64.0 * std::numeric_limits<double>::epsilon();
        return std::abs(x) <= 1.0 + tolerance && std::abs(y) <= 1.0 + tolerance &&
               std::abs(z) <= 1.0 + tolerance && std::abs(x * x + y * y + z * z - 1.0) <= tolerance;
    };
    if (!unit(plane.normalX, plane.normalY, plane.normalZ) ||
        !unit(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ))
        throw std::invalid_argument(
            "Sketch plane axes must have unit length; legacy plane requires explicit migration");
    const double dot = plane.normalX * plane.xDirectionX + plane.normalY * plane.xDirectionY +
                       plane.normalZ * plane.xDirectionZ;
    if (std::abs(dot) > basisTolerance)
        throw std::invalid_argument(
            "Sketch plane axes must be orthogonal; legacy plane requires explicit migration");
    // Y is normal cross X, fixing right-handedness without a mutable third axis.
}
} // namespace voelund::document
