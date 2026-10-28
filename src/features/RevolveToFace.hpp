// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/features/SketchSupportBinding.hpp>

#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>

namespace voelund::features::detail {

// N10: the angle (radians, 0 < angle <= 2 pi) a revolve about `axis` turns its profile
// (around `profileCentre`) until it reaches `face`, a planar face through the axis.
[[nodiscard]] double revolveAngleToFace(const gp_Ax1& axis, const gp_Pnt& profileCentre,
                                        const SupportFaceFrame& face);

} // namespace voelund::features::detail
