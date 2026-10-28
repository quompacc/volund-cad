// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "RevolveToFace.hpp"

#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <cmath>
#include <numbers>

// N10 (agent decision 2026-10-28, as Fusion's revolve "To" extent): a revolve turns until its
// profile reaches a planar body face that contains the axis - the angle from the profile's
// side of the axis to the face's side, in the turning direction (reversed turns the other
// way). A face off the axis is refused with its reason.
namespace voelund::features::detail {
namespace {

// The part of `point - axis location` across the axis.
gp_Vec across(const gp_Ax1& axis, const gp_Pnt& point) {
    const gp_Vec offset(axis.Location(), point);
    const gp_Vec along(axis.Direction());
    return offset - along * offset.Dot(along);
}

} // namespace

double revolveAngleToFace(const gp_Ax1& axis, const gp_Pnt& profileCentre,
                          const SupportFaceFrame& face) {
    const gp_Vec normal(face.plane.normalX, face.plane.normalY, face.plane.normalZ);
    const gp_Pnt origin(face.plane.originX, face.plane.originY, face.plane.originZ);
    const bool containsAxis =
        std::abs(normal.Normalized().Dot(gp_Vec(axis.Direction()))) <
            std::sin(kernel::angularTolerance.radians) &&
        std::abs(gp_Vec(origin, axis.Location()).Dot(normal.Normalized())) < 1e-6;
    if (!containsAxis)
        throw GeometryEvaluationError("A revolve turns up to a planar face through its axis",
                                      "end-face-not-on-axis");
    const gp_Vec from = across(axis, profileCentre);
    const gp_Vec to = across(axis, kernel::boundingBoxCentre(face.face));
    if (from.Magnitude() < 1e-9 || to.Magnitude() < 1e-9)
        throw GeometryEvaluationError("The profile or the end face lies on the revolve axis",
                                      "end-face-not-on-axis");
    double angle = std::atan2(gp_Vec(axis.Direction()).Dot(from.Crossed(to)), from.Dot(to));
    if (angle <= 1e-9)
        angle += 2 * std::numbers::pi;
    return angle;
}

} // namespace voelund::features::detail
