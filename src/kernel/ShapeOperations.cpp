#include "OperationHistoryDetail.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <array>
#include <string>

namespace voelund::kernel {
namespace {

int solidCountOf(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

void requireInput(KernelOperation operation, const TopoDS_Shape& shape, const char* name) {
    if (shape.IsNull()) {
        throw KernelError(operation, std::string(name) + " shape must not be null");
    }
}

void requireValidResult(KernelOperation operation, const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        throw KernelError(operation, "operation produced a null shape");
    }
    if (!BRepCheck_Analyzer(shape).IsValid()) {
        throw KernelError(operation, "operation produced an invalid shape");
    }
}

} // namespace

namespace {
// M05/M07: rounds or chamfers edges with one value; the builder's history names the result.
template <class Builder>
ShapeOperationResult treatEdges(KernelOperation operation, BooleanFailure failure,
                                const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                                double value) {
    requireInput(operation, input, "input");
    if (edges.empty())
        throw KernelError(operation, "the operation needs at least one edge");
    if (!(value > 0.0))
        throw KernelError(operation, "the radius or distance must be positive");
    try {
        Builder builder(input);
        for (const TopoDS_Edge& edge : edges)
            builder.Add(value, edge);
        builder.Build();
        if (!builder.IsDone() || builder.Shape().IsNull() ||
            solidCountOf(builder.Shape()) != solidCountOf(input) ||
            !BRepCheck_Analyzer(builder.Shape()).IsValid())
            throw KernelError(operation, "the edges cannot take this radius or distance", failure);
        ShapeOperationResult result{builder.Shape(), {}, {}};
        detail::appendHistory(result, builder, true, input, HistoryOperand::first);
        return result;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        throw KernelError(operation, occtFailureDiagnostic(error), failure);
    }
}
template <class Treat>
std::vector<std::size_t> failingEdges(std::span<const TopoDS_Edge> edges, Treat treat) {
    std::vector<std::size_t> failing;
    for (std::size_t index = 0; index < edges.size(); ++index) {
        try {
            static_cast<void>(treat(edges.subspan(index, 1)));
        } catch (const KernelError&) {
            failing.push_back(index);
        }
    }
    if (failing.empty())
        for (std::size_t index = 0; index < edges.size(); ++index)
            failing.push_back(index);
    return failing;
}
} // namespace

ShapeOperationResult ShapeOperations::filletWithHistory(const TopoDS_Shape& input,
                                                        std::span<const TopoDS_Edge> edges,
                                                        double radius) {
    return treatEdges<BRepFilletAPI_MakeFillet>(KernelOperation::fillet,
                                                BooleanFailure::filletFailed, input, edges, radius);
}

std::vector<std::size_t> ShapeOperations::failingFilletEdges(const TopoDS_Shape& input,
                                                             std::span<const TopoDS_Edge> edges,
                                                             double radius) {
    return failingEdges(edges, [&](auto one) { return filletWithHistory(input, one, radius); });
}

ShapeOperationResult ShapeOperations::chamferWithHistory(const TopoDS_Shape& input,
                                                         std::span<const TopoDS_Edge> edges,
                                                         double distance) {
    return treatEdges<BRepFilletAPI_MakeChamfer>(
        KernelOperation::chamfer, BooleanFailure::chamferFailed, input, edges, distance);
}

std::vector<std::size_t> ShapeOperations::failingChamferEdges(const TopoDS_Shape& input,
                                                              std::span<const TopoDS_Edge> edges,
                                                              double distance) {
    return failingEdges(edges, [&](auto one) { return chamferWithHistory(input, one, distance); });
}

ShapeOperationResult ShapeOperations::fuseWithHistory(const TopoDS_Shape& left,
                                                      const TopoDS_Shape& right) {
    requireInput(KernelOperation::fuse, left, "left");
    requireInput(KernelOperation::fuse, right, "right");
    try {
        BRepAlgoAPI_Fuse operation;
        detail::configureBoolean(operation, left, right);
        if (!operation.IsDone()) {
            throw KernelError(KernelOperation::fuse,
                              "OCCT failed to join the tool with its input body");
        }
        const TopoDS_Shape result = operation.Shape();
        if (solidCountOf(result) != 1) {
            throw KernelError(KernelOperation::fuse,
                              "join requires the tool to intersect its input body",
                              BooleanFailure::disjointJoin);
        }
        requireValidResult(KernelOperation::fuse, result);
        return detail::booleanResult(operation, left, right);
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::fuse, occtFailureDiagnostic(failure));
    }
}

ShapeOperationResult ShapeOperations::cutWithHistory(const TopoDS_Shape& input,
                                                     const TopoDS_Shape& tool) {
    requireInput(KernelOperation::cut, input, "input");
    requireInput(KernelOperation::cut, tool, "tool");
    try {
        static_cast<void>(intersect(input, tool));
        BRepAlgoAPI_Cut operation;
        detail::configureBoolean(operation, input, tool);
        if (!operation.IsDone()) {
            throw KernelError(KernelOperation::cut,
                              "OCCT failed to cut the tool from its input body");
        }
        const TopoDS_Shape result = operation.Shape();
        if (solidCountOf(result) == 0) {
            throw KernelError(KernelOperation::cut, "cut removed the entire input body",
                              BooleanFailure::wholeBodyRemoved);
        }
        requireValidResult(KernelOperation::cut, result);
        return detail::booleanResult(operation, input, tool);
    } catch (const KernelError& error) {
        if (error.operation() == KernelOperation::cut) {
            throw;
        }
        // M03: a tool that misses the body or only touches it removes no material.
        throw KernelError(KernelOperation::cut,
                          std::string("overlap validation failed: ") + error.what(),
                          error.failure() == BooleanFailure::emptyIntersection
                              ? BooleanFailure::noMaterialRemoved
                              : error.failure());
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::cut, occtFailureDiagnostic(failure));
    }
}

ShapeOperationResult ShapeOperations::intersectWithHistory(const TopoDS_Shape& left,
                                                           const TopoDS_Shape& right) {
    requireInput(KernelOperation::intersect, left, "left");
    requireInput(KernelOperation::intersect, right, "right");
    try {
        BRepAlgoAPI_Common operation;
        detail::configureBoolean(operation, left, right);
        if (!operation.IsDone()) {
            throw KernelError(KernelOperation::intersect,
                              "OCCT failed to intersect the tool with its input body");
        }
        const TopoDS_Shape result = operation.Shape();
        if (solidCountOf(result) == 0) {
            throw KernelError(KernelOperation::intersect,
                              "intersect requires the tool to overlap its input body",
                              BooleanFailure::emptyIntersection);
        }
        requireValidResult(KernelOperation::intersect, result);
        return detail::booleanResult(operation, left, right);
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::intersect, occtFailureDiagnostic(failure));
    }
}

TopoDS_Shape ShapeOperations::compound(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    const std::array shapes{first, second};
    return compound(shapes);
}

TopoDS_Shape ShapeOperations::compound(std::span<const TopoDS_Shape> shapes) {
    if (shapes.empty()) {
        throw KernelError(KernelOperation::compound, "requires at least one shape");
    }
    for (const TopoDS_Shape& shape : shapes) {
        requireInput(KernelOperation::compound, shape, "component");
    }
    try {
        BRep_Builder builder;
        TopoDS_Compound result;
        builder.MakeCompound(result);
        for (const TopoDS_Shape& shape : shapes) {
            builder.Add(result, shape);
        }
        requireValidResult(KernelOperation::compound, result);
        return result;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(KernelOperation::compound, occtFailureDiagnostic(failure));
    }
}

TopoDS_Shape ShapeOperations::fuse(const TopoDS_Shape& left, const TopoDS_Shape& right) {
    return fuseWithHistory(left, right).shape;
}
TopoDS_Shape ShapeOperations::cut(const TopoDS_Shape& input, const TopoDS_Shape& tool) {
    return cutWithHistory(input, tool).shape;
}
TopoDS_Shape ShapeOperations::intersect(const TopoDS_Shape& left, const TopoDS_Shape& right) {
    return intersectWithHistory(left, right).shape;
}
} // namespace voelund::kernel
