#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <numbers>
#include <stdexcept>

namespace voelund::kernel {

TopoDS_Shape ShapeFactory::makeBox(const BoxDimensions& dimensions)
{
    if (dimensions.width <= 0.0 || dimensions.depth <= 0.0 || dimensions.height <= 0.0) {
        throw std::invalid_argument("Box dimensions must be positive");
    }

    return BRepPrimAPI_MakeBox(dimensions.width, dimensions.depth, dimensions.height).Shape();
}

TopoDS_Shape ShapeFactory::makeCylinder(const CylinderDefinition& definition)
{
    if (definition.radius <= 0.0 || definition.height <= 0.0) {
        throw std::invalid_argument("Cylinder radius and height must be positive");
    }

    const gp_Ax2 axis(
        gp_Pnt(definition.originX, definition.originY, definition.originZ),
        gp_Dir(0.0, 0.0, 1.0));
    return BRepPrimAPI_MakeCylinder(axis, definition.radius, definition.height).Shape();
}

TopoDS_Shape ShapeFactory::applyTransform(
    const TopoDS_Shape& source,
    double pivotX,
    double pivotY,
    double pivotZ,
    const RigidTransform& definition)
{
    TopoDS_Shape shape = source;
    const gp_Pnt center(pivotX, pivotY, pivotZ);
    constexpr double degreesToRadians = std::numbers::pi / 180.0;
    struct Rotation final {
        gp_Dir axis;
        double angle;
    };
    const Rotation rotations[] = {
        {gp_Dir(1.0, 0.0, 0.0), definition.rotationX},
        {gp_Dir(0.0, 1.0, 0.0), definition.rotationY},
        {gp_Dir(0.0, 0.0, 1.0), definition.rotationZ}};
    for (const Rotation& rotation : rotations) {
        if (rotation.angle == 0.0) {
            continue;
        }
        gp_Trsf transform;
        transform.SetRotation(
            gp_Ax1(center, rotation.axis),
            rotation.angle * degreesToRadians);
        shape = BRepBuilderAPI_Transform(shape, transform, true).Shape();
    }
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(
        definition.translationX,
        definition.translationY,
        definition.translationZ));
    shape = BRepBuilderAPI_Transform(shape, translation, true).Shape();
    return shape;
}

} // namespace voelund::kernel
