// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/PathFrames.hpp>

#include <BRepAdaptor_CompCurve.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Standard_Failure.hxx>
#include <gp_Vec.hxx>

namespace voelund::kernel {

double wireLength(const TopoDS_Wire& wire) {
    try {
        BRepAdaptor_CompCurve curve(wire);
        return GCPnts_AbscissaPoint::Length(curve);
    } catch (const Standard_Failure& error) {
        throw KernelError(KernelOperation::pattern, occtFailureDiagnostic(error));
    }
}

bool isClosedWire(const TopoDS_Wire& wire) { return BRep_Tool::IsClosed(wire); }

PathFrame wireFrameAt(const TopoDS_Wire& wire, double distance) {
    try {
        BRepAdaptor_CompCurve curve(wire);
        const GCPnts_AbscissaPoint at(curve, distance, curve.FirstParameter());
        if (!at.IsDone())
            throw KernelError(KernelOperation::pattern, "no point at that length of the path");
        gp_Pnt point;
        gp_Vec tangent;
        curve.D1(at.Parameter(), point, tangent);
        if (tangent.Magnitude() < 1e-12)
            throw KernelError(KernelOperation::pattern, "the path has no direction there");
        return {point, gp_Dir(tangent)};
    } catch (const Standard_Failure& error) {
        throw KernelError(KernelOperation::pattern, occtFailureDiagnostic(error));
    }
}

} // namespace voelund::kernel
