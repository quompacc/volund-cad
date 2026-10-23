// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OperationHistoryDetail.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/kernel/Angle.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/NumericValidation.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
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

template <typename Build>
ShapeOperationResult buildShape(KernelOperation operation, Build&& build) {
    try {
        const ShapeOperationResult shape = build();
        requireValidShape(operation, shape.shape);
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

KernelError::KernelError(KernelOperation operation, std::string diagnostic, BooleanFailure failure)
    : std::runtime_error(std::string(kernelOperationName(operation)) + ": " + diagnostic),
      operation_(operation), diagnostic_(std::move(diagnostic)), failure_(failure) {}

KernelOperation KernelError::operation() const noexcept { return operation_; }

const std::string& KernelError::diagnostic() const noexcept { return diagnostic_; }

const char* kernelOperationName(KernelOperation operation) noexcept {
    // In the order of KernelOperation.
    static constexpr const char* names[] = {
        "makeBox",        "makeCylinder", "applyTransform", "fuse",           "cut",
        "intersect",      "compound",     "makePrism",      "fillet",         "chamfer",
        "makeRevolution", "shell",        "makeSphere",     "makeCone",       "makeTorus",
        "makeWedge",      "makePipe",     "loft",           "helix",          "draft",
        "mirror",         "pattern",      "measure",        "sketchGeometry", "nativeDocument",
        "evaluation"};
    const auto index = static_cast<std::size_t>(operation);
    return index < std::size(names) ? names[index] : "unknown";
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

void translateOcctFailures(KernelOperation operation, const std::function<void()>& action) {
    try {
        action();
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(operation, occtFailureDiagnostic(failure));
    }
}

void translateEveryOcctFailure(KernelOperation operation, const std::function<void()>& action) {
    try {
        action();
    } catch (const Standard_Failure& failure) {
        throw KernelError(operation, occtFailureDiagnostic(failure));
    }
}

ShapeOperationResult ShapeFactory::makeBoxWithHistory(const BoxDimensions& dimensions) {
    if (!isFinite(dimensions) || !modelLength(dimensions.width) || !modelLength(dimensions.depth) ||
        !modelLength(dimensions.height)) {
        throw KernelError(KernelOperation::makeBox, modelRangeDiagnostic);
    }
    return buildShape(KernelOperation::makeBox, [&] {
        BRepPrimAPI_MakeBox maker(dimensions.width, dimensions.depth, dimensions.height);
        const auto shape = maker.Shape();
        return ShapeOperationResult{shape,
                                    {{PrimitiveRole::boxNegativeX, maker.BackFace()},
                                     {PrimitiveRole::boxPositiveX, maker.FrontFace()},
                                     {PrimitiveRole::boxNegativeY, maker.LeftFace()},
                                     {PrimitiveRole::boxPositiveY, maker.RightFace()},
                                     {PrimitiveRole::boxNegativeZ, maker.BottomFace()},
                                     {PrimitiveRole::boxPositiveZ, maker.TopFace()}},
                                    {}};
    });
}

ShapeOperationResult ShapeFactory::makeCylinderWithHistory(const CylinderDefinition& definition) {
    if (!isFinite(definition) || !modelLength(definition.radius) ||
        !modelLength(definition.height) || !modelCoordinate(definition.originX) ||
        !modelCoordinate(definition.originY) || !modelCoordinate(definition.originZ)) {
        throw KernelError(KernelOperation::makeCylinder, modelRangeDiagnostic);
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
        BRepPrimAPI_MakeCylinder maker(axis, definition.radius, definition.height);
        const auto shape = maker.Shape();
        return ShapeOperationResult{shape,
                                    {{PrimitiveRole::cylinderStart, maker.Cylinder().BottomFace()},
                                     {PrimitiveRole::cylinderEnd, maker.Cylinder().TopFace()},
                                     {PrimitiveRole::cylinderSide, maker.Cylinder().LateralFace()}},
                                    {}};
    });
}

ShapeOperationResult ShapeFactory::applyTransformWithHistory(const TopoDS_Shape& source,
                                                             double pivotX, double pivotY,
                                                             double pivotZ,
                                                             const RigidTransform& definition) {
    if (source.IsNull()) {
        throw KernelError(KernelOperation::applyTransform, "source shape must not be null");
    }
    if (!modelCoordinate(pivotX) || !modelCoordinate(pivotY) || !modelCoordinate(pivotZ) ||
        !modelCoordinate(definition.translationX) || !modelCoordinate(definition.translationY) ||
        !modelCoordinate(definition.translationZ) || !isFinite(definition)) {
        throw KernelError(KernelOperation::applyTransform, modelRangeDiagnostic);
    }
    return buildShape(KernelOperation::applyTransform, [&] {
        auto shape = detail::transformInput(source);
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
            detail::transformStep(shape, transform);
        }
        gp_Trsf translation;
        translation.SetTranslation(
            gp_Vec(definition.translationX, definition.translationY, definition.translationZ));
        detail::transformStep(shape, translation);
        return shape;
    });
}

ShapeOperationResult ShapeFactory::mirrorWithHistory(const TopoDS_Shape& source,
                                                     const std::array<double, 3>& origin,
                                                     const std::array<double, 3>& normal) {
    if (source.IsNull())
        throw KernelError(KernelOperation::mirror, "source shape must not be null");
    for (const double value : origin)
        if (!modelCoordinate(value))
            throw KernelError(KernelOperation::mirror, modelRangeDiagnostic);
    const double length = std::hypot(normal[0], normal[1], normal[2]);
    if (!std::isfinite(length) || length < 1e-12)
        throw KernelError(KernelOperation::mirror, "mirror plane normal must not be zero");
    return buildShape(KernelOperation::mirror, [&] {
        auto shape = detail::transformInput(source);
        gp_Trsf mirror;
        mirror.SetMirror(gp_Ax2(gp_Pnt(origin[0], origin[1], origin[2]),
                                gp_Dir(normal[0], normal[1], normal[2])));
        detail::transformStep(shape, mirror);
        // A reflection turns the handedness; the copy must still enclose its volume.
        GProp_GProps properties;
        BRepGProp::VolumeProperties(shape.shape, properties);
        if (properties.Mass() < 0.0)
            shape.shape.Reverse();
        return shape;
    });
}

ShapeOperationResult ShapeFactory::copyWithHistory(const TopoDS_Shape& source,
                                                   const std::array<double, 3>& origin,
                                                   const std::array<double, 3>& direction,
                                                   double angle,
                                                   const std::array<double, 3>& translation) {
    if (source.IsNull())
        throw KernelError(KernelOperation::pattern, "source shape must not be null");
    for (const double value :
         {origin[0], origin[1], origin[2], translation[0], translation[1], translation[2]})
        if (!modelCoordinate(value))
            throw KernelError(KernelOperation::pattern, modelRangeDiagnostic);
    const double length = std::hypot(direction[0], direction[1], direction[2]);
    if (!std::isfinite(length) || length < 1e-12 || !std::isfinite(angle))
        throw KernelError(KernelOperation::pattern, "pattern axis must not be zero");
    return buildShape(KernelOperation::pattern, [&] {
        auto shape = detail::transformInput(source);
        gp_Trsf move;
        if (angle != 0.0)
            move.SetRotation(gp_Ax1(gp_Pnt(origin[0], origin[1], origin[2]),
                                    gp_Dir(direction[0], direction[1], direction[2])),
                             angle);
        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(translation[0], translation[1], translation[2]));
        // One copy step, so the copy is a new shape even for a pure translation.
        detail::transformStep(shape, shift * move);
        return shape;
    });
}

TopoDS_Shape ShapeFactory::makeBox(const BoxDimensions& dimensions) {
    return makeBoxWithHistory(dimensions).shape;
}
TopoDS_Shape ShapeFactory::makeCylinder(const CylinderDefinition& definition) {
    return makeCylinderWithHistory(definition).shape;
}
TopoDS_Shape ShapeFactory::applyTransform(const TopoDS_Shape& shape, double x, double y, double z,
                                          const RigidTransform& transform) {
    return applyTransformWithHistory(shape, x, y, z, transform).shape;
}
} // namespace voelund::kernel
