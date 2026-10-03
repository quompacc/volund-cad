#pragma once
#include <Geom2d_Curve.hxx>
#include <voelund/features/SketchContourGraph.hpp>
namespace voelund::features::detail {
struct PlanarCurve final {
    Handle(Geom2d_Curve) curve;
    double first;
    double last;
    bool periodic;
    double scale;
};
[[nodiscard]] PlanarCurve planarCurve(const ContourPiece& piece);
[[nodiscard]] double parameterTolerance(const PlanarCurve& curve, double parameter,
                                        double distance);
[[nodiscard]] double domainParameter(const PlanarCurve& curve, double parameter, double distance);
} // namespace voelund::features::detail
