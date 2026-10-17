#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <Geom2d_BSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>

#include <algorithm>

// S21: the curve of a sketch B-spline: cubic (lower for fewer control points), clamped,
// uniform knots 0, 1, ..., n - degree. The 2D and the 3D curve share their parameters.
namespace voelund::features::detail {

inline int bsplineDegree(const document::SketchBSpline& spline) {
    return std::min(3, static_cast<int>(spline.poles.size()) - 1);
}

inline double bsplineLastParameter(const document::SketchBSpline& spline) {
    return static_cast<double>(spline.poles.size()) - bsplineDegree(spline);
}

inline void bsplineKnots(const document::SketchBSpline& spline, NCollection_Array1<double>& knots,
                         NCollection_Array1<int>& multiplicities) {
    const int degree = bsplineDegree(spline);
    for (int index = knots.Lower(); index <= knots.Upper(); ++index) {
        knots.SetValue(index, index - knots.Lower());
        multiplicities.SetValue(index,
                                index == knots.Lower() || index == knots.Upper() ? degree + 1 : 1);
    }
}

inline Handle(Geom2d_BSplineCurve) bsplineCurve2d(const document::SketchBSpline& spline) {
    const int count = static_cast<int>(spline.poles.size());
    NCollection_Array1<gp_Pnt2d> poles(1, count);
    for (int index = 0; index < count; ++index)
        poles.SetValue(index + 1, gp_Pnt2d(spline.poles[static_cast<std::size_t>(index)].x,
                                           spline.poles[static_cast<std::size_t>(index)].y));
    const int spans = static_cast<int>(bsplineLastParameter(spline));
    NCollection_Array1<double> knots(1, spans + 1);
    NCollection_Array1<int> multiplicities(1, spans + 1);
    bsplineKnots(spline, knots, multiplicities);
    return new Geom2d_BSplineCurve(poles, knots, multiplicities, bsplineDegree(spline));
}

inline Handle(Geom_BSplineCurve)
    bsplineCurve3d(const document::SketchBSpline& spline, const document::SketchPlane& plane) {
    const gp_Vec normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Vec x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec y = normal.Crossed(x);
    const gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
    const int count = static_cast<int>(spline.poles.size());
    NCollection_Array1<gp_Pnt> poles(1, count);
    for (int index = 0; index < count; ++index) {
        const auto& pole = spline.poles[static_cast<std::size_t>(index)];
        poles.SetValue(index + 1, origin.Translated(x * pole.x + y * pole.y));
    }
    const int spans = static_cast<int>(bsplineLastParameter(spline));
    NCollection_Array1<double> knots(1, spans + 1);
    NCollection_Array1<int> multiplicities(1, spans + 1);
    bsplineKnots(spline, knots, multiplicities);
    return new Geom_BSplineCurve(poles, knots, multiplicities, bsplineDegree(spline));
}

} // namespace voelund::features::detail
