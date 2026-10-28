// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

// N12a: one side of a split. The block reaches beyond the body's box by its diagonal in every
// direction along the plane and away from it, so its common part with the body is all of that
// side.
namespace voelund::kernel {

TopoDS_Shape ShapeFactory::sideBlock(const TopoDS_Shape& around,
                                     const std::array<double, 3>& origin,
                                     const std::array<double, 3>& normal, bool front) {
    if (around.IsNull())
        throw KernelError(KernelOperation::intersect, "a split needs a body");
    try {
        Bnd_Box box;
        BRepBndLib::Add(around, box);
        const gp_Pnt at(origin[0], origin[1], origin[2]);
        gp_Dir direction(normal[0], normal[1], normal[2]);
        if (!front)
            direction.Reverse();
        // Far enough: the box's diagonal plus the plane's distance from it.
        const double reach =
            std::sqrt(box.SquareExtent()) + std::sqrt(box.CornerMin().SquareDistance(at)) + 1.0;
        const gp_Ax2 frame(at, direction);
        const gp_Pnt corner = at.Translated(gp_Vec(frame.XDirection()) * -reach +
                                            gp_Vec(frame.YDirection()) * -reach);
        return BRepPrimAPI_MakeBox(gp_Ax2(corner, direction, frame.XDirection()), 2 * reach,
                                   2 * reach, reach)
            .Shape();
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::intersect, occtFailureDiagnostic(failure));
    }
}

} // namespace voelund::kernel
