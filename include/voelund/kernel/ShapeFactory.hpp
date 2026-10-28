// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/Angle.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Shape.hxx>

#include <array>

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
    // Internal rotations are radians; legacy files and UI convert explicitly.
    Radians rotationX;
    Radians rotationY;
    Radians rotationZ;

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
    [[nodiscard]] static ShapeOperationResult makeBoxWithHistory(const BoxDimensions& dimensions);
    [[nodiscard]] static ShapeOperationResult
    makeCylinderWithHistory(const CylinderDefinition& definition);
    [[nodiscard]] static ShapeOperationResult
    applyTransformWithHistory(const TopoDS_Shape& shape, double pivotX, double pivotY,
                              double pivotZ, const RigidTransform& transform);
    // M09: the mirror image of `shape` about the plane through `origin` with `normal`, as a
    // copy with history (HistoryOperand::first). The copy encloses a positive volume.
    [[nodiscard]] static ShapeOperationResult
    mirrorWithHistory(const TopoDS_Shape& shape, const std::array<double, 3>& origin,
                      const std::array<double, 3>& normal);
    // N12a (SplitBlock.cpp): a block that holds all of `around` on one side of the plane through
    // `origin` with `normal` (the front: where the normal points) and ends at the plane; its
    // common part with `around` is that side of it.
    [[nodiscard]] static TopoDS_Shape sideBlock(const TopoDS_Shape& around,
                                                const std::array<double, 3>& origin,
                                                const std::array<double, 3>& normal, bool front);
    // M10: a copy of `shape` turned by `angle` (radians) about the axis through `origin` along
    // `direction`, then moved by `translation`, with history (HistoryOperand::first).
    [[nodiscard]] static ShapeOperationResult
    copyWithHistory(const TopoDS_Shape& shape, const std::array<double, 3>& origin,
                    const std::array<double, 3>& direction, double angle,
                    const std::array<double, 3>& translation);
    // All entry points reject invalid inputs and throw KernelError when OCCT
    // rejects an operation or produces an invalid result.
    [[nodiscard]] static TopoDS_Shape makeBox(const BoxDimensions& dimensions);
    [[nodiscard]] static TopoDS_Shape makeCylinder(const CylinderDefinition& definition);
    [[nodiscard]] static TopoDS_Shape applyTransform(const TopoDS_Shape& shape, double pivotX,
                                                     double pivotY, double pivotZ,
                                                     const RigidTransform& transform);
};

} // namespace voelund::kernel
