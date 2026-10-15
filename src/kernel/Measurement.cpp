#include <voelund/kernel/Measurement.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/PlaneNormal.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Dir.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace voelund::kernel {
namespace {

bool hasSolid(const TopoDS_Shape& shape) {
    return TopExp_Explorer(shape, TopAbs_SOLID).More();
}

// A line's direction or a plane's normal; a line and a plane are compared through the normal.
struct Direction final {
    gp_Dir value;
    bool normal;
};
std::optional<Direction> directionOf(const TopoDS_Shape& shape) {
    if (shape.ShapeType() == TopAbs_EDGE) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        if (curve.GetType() == GeomAbs_Line)
            return Direction{curve.Line().Direction(), false};
    } else if (shape.ShapeType() == TopAbs_FACE) {
        const BRepAdaptor_Surface surface(TopoDS::Face(shape));
        if (surface.GetType() == GeomAbs_Plane)
            return Direction{planeNormal(surface.Plane(), shape.Orientation()), true};
    }
    return std::nullopt;
}

} // namespace

ShapeMeasure measureShape(const TopoDS_Shape& shape) {
    ShapeMeasure result;
    if (shape.IsNull())
        return result;
    GProp_GProps properties;
    switch (shape.ShapeType()) {
    case TopAbs_EDGE: {
        BRepGProp::LinearProperties(shape, properties);
        result.length = properties.Mass();
        const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        if (curve.GetType() == GeomAbs_Circle)
            result.radius = curve.Circle().Radius();
        break;
    }
    case TopAbs_FACE: {
        BRepGProp::SurfaceProperties(shape, properties);
        result.area = properties.Mass();
        const BRepAdaptor_Surface surface(TopoDS::Face(shape));
        if (surface.GetType() == GeomAbs_Cylinder)
            result.radius = surface.Cylinder().Radius();
        else if (surface.GetType() == GeomAbs_Sphere)
            result.radius = surface.Sphere().Radius();
        break;
    }
    default:
        if (hasSolid(shape)) {
            BRepGProp::VolumeProperties(shape, properties);
            result.volume = std::abs(properties.Mass());
        }
        break;
    }
    return result;
}

PairMeasure measureBetween(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    BRepExtrema_DistShapeShape extrema(first, second);
    if (!extrema.IsDone())
        throw KernelError(KernelOperation::measure, "distance cannot be measured");
    PairMeasure result{extrema.Value()};
    const auto a = directionOf(first);
    const auto b = directionOf(second);
    if (a && b) {
        // Undirected: lines and planes have no sense, so the angle lies in 0..90 degrees.
        const double between = a->value.Angle(b->value);
        double angle = std::min(between, std::numbers::pi - between);
        if (a->normal != b->normal) // a line against a plane: against the plane, not its normal
            angle = std::numbers::pi / 2 - angle;
        result.angle = angle;
    }
    return result;
}

} // namespace voelund::kernel
