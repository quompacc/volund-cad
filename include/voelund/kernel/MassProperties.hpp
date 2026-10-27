// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>

#include <array>
#include <span>

namespace voelund::kernel {

// N07: the geometric mass properties of solids, in mm: volume (mm³), surface area (mm²), the
// centroid and the inertia tensor about the centroid per unit density (mm⁵; times a density
// in g/mm³ it is g·mm²), ordered xx, yy, zz, xy, xz, yz. An empty shape has zero volume and
// its centroid at the origin.
struct MassProperties final {
    double volume{0};
    double area{0};
    std::array<double, 3> centroid{};
    std::array<double, 6> inertia{};
};
[[nodiscard]] MassProperties massProperties(const TopoDS_Shape& shape);

// N07: several bodies with their densities (g/cm³) together: the total mass in g, the common
// centre of mass and the inertia about it in g·mm² (parallel axis theorem).
struct CombinedMass final {
    double mass{0};
    double volume{0};
    double area{0};
    std::array<double, 3> center{};
    std::array<double, 6> inertia{};
};
struct WeightedBody final {
    MassProperties properties;
    double density; // g/cm³
};
[[nodiscard]] CombinedMass combineMasses(std::span<const WeightedBody> bodies);

} // namespace voelund::kernel
