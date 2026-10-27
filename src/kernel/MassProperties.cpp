// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/MassProperties.hpp>

#include "OcctGuard.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Mat.hxx>

namespace voelund::kernel {

MassProperties massProperties(const TopoDS_Shape& shape) {
    return detail::guarded(KernelOperation::evaluation, [&] {
        MassProperties result;
        if (shape.IsNull())
            return result;
        GProp_GProps volume;
        BRepGProp::VolumeProperties(shape, volume);
        GProp_GProps surface;
        BRepGProp::SurfaceProperties(shape, surface);
        result.volume = volume.Mass();
        result.area = surface.Mass();
        if (result.volume <= 0)
            return result;
        const gp_Pnt center = volume.CentreOfMass();
        result.centroid = {center.X(), center.Y(), center.Z()};
        // OCCT gives the matrix of inertia about the centre of mass (diagonal: moments,
        // off-diagonal: negated products of inertia).
        const gp_Mat matrix = volume.MatrixOfInertia();
        result.inertia = {matrix(1, 1), matrix(2, 2), matrix(3, 3),
                          matrix(1, 2), matrix(1, 3), matrix(2, 3)};
        return result;
    });
}

CombinedMass combineMasses(std::span<const WeightedBody> bodies) {
    CombinedMass result;
    std::array<double, 3> moment{};
    for (const auto& [properties, density] : bodies) {
        const double mass = properties.volume * density / 1000.0; // mm³ · g/cm³ → g
        result.mass += mass;
        result.volume += properties.volume;
        result.area += properties.area;
        for (std::size_t axis = 0; axis < 3; ++axis)
            moment[axis] += mass * properties.centroid[axis];
    }
    if (result.mass <= 0)
        return result;
    for (std::size_t axis = 0; axis < 3; ++axis)
        result.center[axis] = moment[axis] / result.mass;
    // Each body's tensor about its centroid, moved to the common centre (Steiner).
    for (const auto& [properties, density] : bodies) {
        const double mass = properties.volume * density / 1000.0;
        const double scale = density / 1000.0; // g/mm³
        const double dx = properties.centroid[0] - result.center[0];
        const double dy = properties.centroid[1] - result.center[1];
        const double dz = properties.centroid[2] - result.center[2];
        const std::array<double, 6> shift{mass * (dy * dy + dz * dz),
                                          mass * (dx * dx + dz * dz),
                                          mass * (dx * dx + dy * dy),
                                          -mass * dx * dy,
                                          -mass * dx * dz,
                                          -mass * dy * dz};
        for (std::size_t index = 0; index < 6; ++index)
            result.inertia[index] += properties.inertia[index] * scale + shift[index];
    }
    return result;
}

} // namespace voelund::kernel
