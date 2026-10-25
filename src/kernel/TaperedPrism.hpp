// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include <numbers>
#include <vector>

namespace voelund::kernel {

inline constexpr double maximumTaper = 89.0 * std::numbers::pi / 180.0;

// W16: the tapered sweep behind Prism. Each profile face is lofted (ruled) from its wires to
// their offset copies at the end - outer wires outwards for a positive taper, holes inwards -
// and its holes are cut out. History is found by place: a cap lies on the start or end plane,
// a side face starts on the edge it was swept from. Throws KernelError when the taper closes
// a profile or the loft fails.
class TaperedPrism final {
  public:
    TaperedPrism(const TopoDS_Shape& profile, const gp_Vec& sweep, double taper);

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept { return shape_; }
    // The copy of a profile face or edge on the start (end == false) or end cap.
    [[nodiscard]] TopoDS_Shape onCap(const TopoDS_Shape& profileShape, bool end) const;
    // The side faces a profile edge sweeps into (one per profile face that uses it).
    [[nodiscard]] std::vector<TopoDS_Shape> sides(const TopoDS_Shape& profileShape) const;

  private:
    struct Part final {
        TopoDS_Face profile;
        TopoDS_Shape solid;
    };
    [[nodiscard]] TopoDS_Shape capEdge(const Part& part, const TopoDS_Shape& edge, bool end) const;

    std::vector<Part> parts_;
    gp_Pln start_;
    gp_Pln end_;
    TopoDS_Shape shape_;
};

} // namespace voelund::kernel
