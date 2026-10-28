// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/Rib.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

#include <cmath>

namespace voelund::kernel {
namespace {

Bnd_Box boxOf(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    return box;
}

// `inner` within `outer`, up to a small share of the outer box's size.
bool within(const Bnd_Box& inner, const Bnd_Box& outer) {
    const double tolerance = 1e-6 * std::sqrt(outer.SquareExtent()) + 1e-7;
    double ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
    double cx = 0, cy = 0, cz = 0, dx = 0, dy = 0, dz = 0;
    inner.Get(ax, ay, az, bx, by, bz);
    outer.Get(cx, cy, cz, dx, dy, dz);
    return ax >= cx - tolerance && ay >= cy - tolerance && az >= cz - tolerance &&
           bx <= dx + tolerance && by <= dy + tolerance && bz <= dz + tolerance;
}

TopoDS_Shape slab(const RibLine& line, double thickness, double reach) {
    const gp_Vec along(line.start, line.end);
    const gp_Dir direction(along);
    gp_Pnt corner =
        line.start.Translated(gp_Vec(direction) * -reach + gp_Vec(line.normal) * (-thickness / 2));
    // The frame's x runs along the line, its y across it, its z along the normal; a side
    // against its y starts the box that far back.
    const gp_Dir across = line.normal.Crossed(direction);
    if (across.Dot(line.side) < 0)
        corner.Translate(gp_Vec(across) * -reach);
    const gp_Ax2 frame(corner, line.normal, direction);
    return BRepPrimAPI_MakeBox(frame, along.Magnitude() + 2 * reach, reach, thickness).Shape();
}

} // namespace

TopoDS_Shape ribMaterial(const TopoDS_Shape& body, const RibLine& line, double thickness) {
    if (body.IsNull() || !(thickness > 0.0) || line.start.Distance(line.end) <= 0.0)
        throw KernelError(KernelOperation::makePrism, "a rib needs a body, a line and a thickness");
    try {
        const Bnd_Box bounds = boxOf(body);
        const double reach = 2 * std::sqrt(bounds.SquareExtent()) + 1.0;
        BRepAlgoAPI_Cut cut(slab(line, thickness, reach), body);
        if (!cut.IsDone())
            throw KernelError(KernelOperation::makePrism, "OCCT failed to cut the rib's slab");
        TopoDS_Compound pieces;
        BRep_Builder builder;
        builder.MakeCompound(pieces);
        bool found = false;
        for (TopExp_Explorer solid(cut.Shape(), TopAbs_SOLID); solid.More(); solid.Next())
            if (within(boxOf(solid.Current()), bounds)) {
                builder.Add(pieces, solid.Current());
                found = true;
            }
        if (!found)
            throw KernelError(KernelOperation::makePrism,
                              "the rib meets no faces of the body on that side");
        return pieces;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::makePrism, occtFailureDiagnostic(failure));
    }
}

} // namespace voelund::kernel
