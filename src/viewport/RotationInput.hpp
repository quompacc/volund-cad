#pragma once
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <optional>
#include <voelund/kernel/Angle.hpp>

namespace voelund::viewport::detail {
// OCCT returns a principal angle plus a signed axis. The UI delta is degrees;
// complete turns belong to the document placement, not this single drag.
inline std::optional<kernel::Degrees> rotationDeltaDegrees(const gp_Trsf& transform,
                                                           int axisIndex) {
    if (axisIndex < 0 || axisIndex > 2)
        return std::nullopt;
    gp_XYZ axis;
    double radians = 0.0;
    if (!transform.GetRotation(axis, radians))
        return std::nullopt;
    const double component = axisIndex == 0 ? axis.X() : axisIndex == 1 ? axis.Y() : axis.Z();
    return kernel::toDegrees(kernel::Radians(radians * (component < 0 ? -1 : 1)));
}
inline gp_Trsf snapRotation(const gp_Trsf& transform, const gp_Pnt& pivot, kernel::Degrees step) {
    const double radiansStep = kernel::toRadians(step).value();
    if (radiansStep <= 0)
        throw std::invalid_argument("Angular snap step must be positive");
    gp_XYZ axis;
    double radians = 0.0;
    if (!transform.GetRotation(axis, radians))
        return transform;
    gp_Trsf result;
    result.SetRotation(gp_Ax1(pivot, gp_Dir(axis)),
                       std::round(radians / radiansStep) * radiansStep);
    return result;
}
} // namespace voelund::viewport::detail
