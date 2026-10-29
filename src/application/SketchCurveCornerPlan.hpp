// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "SketchFilletPlan.hpp"

#include <voelund/document/SketchGeometry.hpp>

// N14a (agent decision 2026-10-29): the geometry of a fillet or chamfer where a line and an
// arc, or two arcs, meet at a common end (the corner). Each curve keeps its other end and is cut
// back to where the fillet arc touches it tangentially (the centre lies `size` from both on the
// inner side) or, for a chamfer, `size` (straight) from the corner. Anything that cannot be
// treated throws SketchEditRefusal naming the curves.
namespace voelund::application::detail {

struct CornerCurve final {
    document::SketchElementId id;
    document::SketchGeometry geometry; // a SketchLine or SketchArc
};

struct CurveCornerLeg final {
    document::SketchElementId id;
    document::SketchGeometry geometry;   // its new geometry, ending at the joint
    document::SketchPointPosition moved; // the end that now meets the fillet or chamfer
};

struct CurveCornerPlan final {
    CurveCornerLeg first, second;
    document::SketchArc arc;      // fillet
    bool arcStartOnFirst;         // the arc starts where the first curve ends on it
    document::SketchLine chamfer; // from the first curve's joint to the second's
};

[[nodiscard]] CurveCornerPlan planCurveCorner(SketchCornerKind kind, const CornerCurve& first,
                                              const CornerCurve& second, double size);

} // namespace voelund::application::detail
