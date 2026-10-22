// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OperationHistory.hpp>

// M27 (user decision 2026-10-14): further primitive solids. Each stands on the plane
// z = originZ, centred on (originX, originY), and names its faces: a cone (or frustum) like a
// cylinder (start = base, end = top unless it ends in an apex, side), a wedge like a box, a
// sphere and a torus by their one surface.
namespace voelund::kernel {

enum class PrimitiveShape { sphere, cone, torus, wedge };

struct SolidPrimitiveDefinition final {
    PrimitiveShape shape;
    // sphere: radius; cone: base radius, top radius, height; torus: ring radius, tube radius;
    // wedge: width (x), depth (y), height (z), top width (x, above 0, at most the width).
    double first;
    double second{0.0};
    double third{0.0};
    double fourth{0.0};
    double originX{0.0};
    double originY{0.0};
    double originZ{0.0};
};

// Why the sizes cannot make a solid; none when they can.
[[nodiscard]] const char* primitiveSizeProblem(const SolidPrimitiveDefinition& definition) noexcept;
// The extent along z, so a placement can pivot about the middle.
[[nodiscard]] double primitiveHeight(const SolidPrimitiveDefinition& definition) noexcept;
// Throws KernelError on invalid sizes or a failing kernel.
[[nodiscard]] ShapeOperationResult
makeSolidPrimitiveWithHistory(const SolidPrimitiveDefinition& definition);

} // namespace voelund::kernel
