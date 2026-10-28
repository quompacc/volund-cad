// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OperationHistoryDetail.hpp"
#include "ShapeValidity.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepFilletAPI_MakeChamfer.hxx>
#include <TopExp_Explorer.hxx>

#include <cmath>
#include <numbers>

// N10 (agent decision 2026-10-28, as Fusion's chamfer types): a chamfer with two distances or
// with a distance and an angle, measured from a chosen face beside each edge. OCCT builds it
// in one step (BRepFilletAPI_MakeChamfer::Add with two distances, AddDA); the robust ways of
// the equal chamfer (one by one, bridges, a cut wedge) are not tried for it.
namespace voelund::kernel {
namespace {

int solids(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next())
        ++count;
    return count;
}

using UnequalChamfer = ShapeOperations::UnequalChamfer;

void requireChamfer(const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                    std::span<const TopoDS_Face> faces, const UnequalChamfer& chamfer) {
    if (input.IsNull() || edges.empty() || edges.size() != faces.size())
        throw KernelError(KernelOperation::chamfer, "a chamfer needs edges, each with its face");
    if (!(chamfer.first > 0.0) || !(chamfer.second > 0.0) ||
        (chamfer.angle && !(chamfer.second < 90.0)))
        throw KernelError(KernelOperation::chamfer,
                          "the distances must be positive and the angle below 90 degrees");
}

TopoDS_Shape built(const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                   std::span<const TopoDS_Face> faces, const UnequalChamfer& chamfer,
                   BRepFilletAPI_MakeChamfer& builder) {
    for (std::size_t index = 0; index < edges.size(); ++index)
        if (chamfer.angle)
            builder.AddDA(chamfer.first, chamfer.second * std::numbers::pi / 180.0, edges[index],
                          faces[index]);
        else
            builder.Add(chamfer.first, chamfer.second, edges[index], faces[index]);
    builder.Build();
    if (!builder.IsDone() || builder.Shape().IsNull() || solids(builder.Shape()) != solids(input) ||
        !detail::isValidShape(builder.Shape()))
        throw KernelError(KernelOperation::chamfer,
                          "the edges cannot take these distances or this angle",
                          BooleanFailure::chamferFailed);
    return builder.Shape();
}

} // namespace

ShapeOperationResult ShapeOperations::chamferUnequalWithHistory(const TopoDS_Shape& input,
                                                                std::span<const TopoDS_Edge> edges,
                                                                std::span<const TopoDS_Face> faces,
                                                                const UnequalChamfer& chamfer) {
    requireChamfer(input, edges, faces, chamfer);
    try {
        BRepFilletAPI_MakeChamfer builder(input);
        ShapeOperationResult result{built(input, edges, faces, chamfer, builder), {}, {}};
        detail::appendHistory(result, builder, true, input, HistoryOperand::first);
        return result;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        throw KernelError(KernelOperation::chamfer, occtFailureDiagnostic(error),
                          BooleanFailure::chamferFailed);
    }
}

} // namespace voelund::kernel
