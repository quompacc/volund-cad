#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

namespace voelund::kernel {

struct BoxDimensions final {
    double width;
    double depth;
    double height;

    [[nodiscard]] bool operator==(const BoxDimensions&) const = default;
};

struct RigidTransform final {
    double translationX;
    double translationY;
    double translationZ;
    double rotationX;
    double rotationY;
    double rotationZ;

    [[nodiscard]] bool operator==(const RigidTransform&) const = default;
};

struct CylinderDefinition final {
    double radius;
    double height;
    double originX;
    double originY;
    double originZ;
    double axisX{0.0};
    double axisY{0.0};
    double axisZ{1.0};
};

class ShapeFactory final {
  public:
    // All entry points reject invalid inputs and throw KernelError when OCCT
    // rejects an operation or produces an invalid result.
    [[nodiscard]] static TopoDS_Shape makeBox(const BoxDimensions& dimensions);
    [[nodiscard]] static TopoDS_Shape makeCylinder(const CylinderDefinition& definition);
    [[nodiscard]] static TopoDS_Shape applyTransform(const TopoDS_Shape& shape, double pivotX,
                                                     double pivotY, double pivotZ,
                                                     const RigidTransform& transform);
};

} // namespace voelund::kernel
