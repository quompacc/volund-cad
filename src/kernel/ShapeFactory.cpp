#include <voelund/kernel/NumericPolicy.hpp>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/kernel/Angle.hpp>
#include <voelund/kernel/NumericValidation.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace voelund::kernel {
namespace {

bool isFinite(double value) { return std::isfinite(value); }

bool isFinite(const BoxDimensions& dimensions) {
    return isFinite(dimensions.width) && isFinite(dimensions.depth) && isFinite(dimensions.height);
}

bool isFinite(const CylinderDefinition& definition) {
    return isFinite(definition.radius) && isFinite(definition.height) &&
           isFinite(definition.originX) && isFinite(definition.originY) &&
           isFinite(definition.originZ) && isFinite(definition.axisX) &&
           isFinite(definition.axisY) && isFinite(definition.axisZ);
}

bool isFinite(const RigidTransform& transform) {
    return isFinite(transform.translationX) && isFinite(transform.translationY) &&
           isFinite(transform.translationZ) && isFinite(transform.rotationX.value()) &&
           isFinite(transform.rotationY.value()) && isFinite(transform.rotationZ.value());
}

void requireValidShape(KernelOperation operation, const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        throw KernelError(operation, "operation produced a null shape");
    }
    if (!BRepCheck_Analyzer(shape).IsValid()) {
        throw KernelError(operation, "operation produced an invalid shape");
    }
}

template <typename Build> TopoDS_Shape buildShape(KernelOperation operation, Build&& build) {
    try {
        const TopoDS_Shape shape = build();
        requireValidShape(operation, shape);
        return shape;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(operation, occtFailureDiagnostic(failure));
    }
}

} // namespace

KernelError::KernelError(KernelOperation operation, std::string diagnostic)
    : std::runtime_error(std::string(kernelOperationName(operation)) + ": " + diagnostic),
      operation_(operation), diagnostic_(std::move(diagnostic)) {}

KernelOperation KernelError::operation() const noexcept { return operation_; }

const std::string& KernelError::diagnostic() const noexcept { return diagnostic_; }

const char* kernelOperationName(KernelOperation operation) noexcept {
    switch (operation) {
    case KernelOperation::makeBox:
        return "makeBox";
    case KernelOperation::makeCylinder:
        return "makeCylinder";
    case KernelOperation::applyTransform:
        return "applyTransform";
    case KernelOperation::fuse:
        return "fuse";
    case KernelOperation::cut:
        return "cut";
    case KernelOperation::intersect:
        return "intersect";
    case KernelOperation::compound:
        return "compound";
    }
    return "unknown";
}

std::string occtFailureDiagnostic(const Standard_Failure& failure) {
#if OCC_VERSION_MAJOR >= 8
    const char* message = failure.what();
#else
    const char* message = failure.GetMessageString();
#endif
    return message != nullptr && message[0] != '\0' ? message
                                                    : "OCCT failure without diagnostic message";
}

TopoDS_Shape ShapeFactory::makeBox(const BoxDimensions& dimensions) {
    if (!isFinite(dimensions) || !modelLength(dimensions.width) || !modelLength(dimensions.depth) || !modelLength(dimensions.height)) {
        throw KernelError(KernelOperation::makeBox, modelRangeDiagnostic);
    }
    return buildShape(KernelOperation::makeBox, [&] {
        return BRepPrimAPI_MakeBox(dimensions.width, dimensions.depth, dimensions.height).Shape();
    });
}

TopoDS_Shape ShapeFactory::makeCylinder(const CylinderDefinition& definition) {
    if (!isFinite(definition) || !modelLength(definition.radius) || !modelLength(definition.height) ||
        !modelCoordinate(definition.originX) || !modelCoordinate(definition.originY) ||
        !modelCoordinate(definition.originZ)) {
        throw KernelError(KernelOperation::makeCylinder,
                          modelRangeDiagnostic);
    }
    return buildShape(KernelOperation::makeCylinder, [&] {
        const auto direction = [&] {
            try {
                return UnitDirection::fromComponents(definition.axisX, definition.axisY,
                                                     definition.axisZ);
            } catch (const std::invalid_argument& error) {
                throw KernelError(KernelOperation::makeCylinder,
                                  std::string("axis: ") + error.what());
            }
        }();
        const gp_Ax2 axis(gp_Pnt(definition.originX, definition.originY, definition.originZ),
                          gp_Dir(direction.x(), direction.y(), direction.z()));
        return BRepPrimAPI_MakeCylinder(axis, definition.radius, definition.height).Shape();
    });
}

TopoDS_Shape ShapeFactory::applyTransform(const TopoDS_Shape& source, double pivotX, double pivotY,
                                          double pivotZ, const RigidTransform& definition) {
    if (source.IsNull()) {
        throw KernelError(KernelOperation::applyTransform, "source shape must not be null");
    }
    if (!modelCoordinate(pivotX) || !modelCoordinate(pivotY) || !modelCoordinate(pivotZ) ||
        !modelCoordinate(definition.translationX) || !modelCoordinate(definition.translationY) ||
        !modelCoordinate(definition.translationZ) || !isFinite(definition)) {
        throw KernelError(KernelOperation::applyTransform, modelRangeDiagnostic);
    }
    return buildShape(KernelOperation::applyTransform, [&] {
        TopoDS_Shape shape = source;
        const gp_Pnt center(pivotX, pivotY, pivotZ);
        struct Rotation final {
            gp_Dir axis;
            Radians angle;
        };
        const Rotation rotations[] = {{gp_Dir(1.0, 0.0, 0.0), definition.rotationX},
                                      {gp_Dir(0.0, 1.0, 0.0), definition.rotationY},
                                      {gp_Dir(0.0, 0.0, 1.0), definition.rotationZ}};
        for (const Rotation& rotation : rotations) {
            if (rotation.angle.value() == 0.0) {
                continue;
            }
            gp_Trsf transform;
            transform.SetRotation(gp_Ax1(center, rotation.axis), rotation.angle.value());
            shape = BRepBuilderAPI_Transform(shape, transform, true).Shape();
        }
        gp_Trsf translation;
        translation.SetTranslation(
            gp_Vec(definition.translationX, definition.translationY, definition.translationZ));
        shape = BRepBuilderAPI_Transform(shape, translation, true).Shape();
        return shape;
    });
}

} // namespace voelund::kernel
