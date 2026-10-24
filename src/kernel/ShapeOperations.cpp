// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OperationHistoryDetail.hpp"
#include "ShapeValidity.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/PlaneNormal.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_Curve.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <optional>

#include <array>
#include <string>

namespace voelund::kernel {
namespace {
using AncestorMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

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
    if (!detail::isValidShape(shape)) {
        throw KernelError(operation, "operation produced an invalid shape");
    }
}

} // namespace

namespace {
void requireEdgeValues(KernelOperation operation, const TopoDS_Shape& input,
                       std::span<const TopoDS_Edge> edges, std::span<const double> values) {
    requireInput(operation, input, "input");
    if (edges.empty() || values.size() != edges.size())
        throw KernelError(operation, "the operation needs at least one edge, each with a value");
    if (!std::ranges::all_of(values, [](double value) { return value > 0.0; }))
        throw KernelError(operation, "the radius or distance must be positive");
}

// M05/M07: rounds or chamfers each edge with its own value; the builder's history names the
// result.
template <class Builder>
ShapeOperationResult treatEdgesEach(KernelOperation operation, BooleanFailure failure,
                                    const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                                    std::span<const double> values) {
    requireEdgeValues(operation, input, edges, values);
    try {
        Builder builder(input);
        for (std::size_t index = 0; index < edges.size(); ++index)
            builder.Add(values[index], edges[index]);
        builder.Build();
        if (!builder.IsDone() || builder.Shape().IsNull() ||
            solidCountOf(builder.Shape()) != solidCountOf(input) ||
            !detail::isValidShape(builder.Shape()))
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
// The edges of `shape` that are pieces of `original` (shortened or split by an earlier step):
// their middle lies on the original curve within its range.
std::vector<TopoDS_Edge> piecesOf(const TopoDS_Shape& shape, const TopoDS_Edge& original) {
    std::vector<TopoDS_Edge> pieces;
    double first = 0.0, last = 0.0;
    const Handle(Geom_Curve) curve = BRep_Tool::Curve(original, first, last);
    if (curve.IsNull())
        return pieces;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge candidate = TopoDS::Edge(explorer.Current());
        if (BRep_Tool::Degenerated(candidate) ||
            std::ranges::any_of(pieces, [&](const auto& known) { return known.IsSame(candidate); }))
            continue;
        const BRepAdaptor_Curve adaptor(candidate);
        const gp_Pnt middle =
            adaptor.Value((adaptor.FirstParameter() + adaptor.LastParameter()) / 2);
        GeomAPI_ProjectPointOnCurve projection(middle, curve, first, last);
        if (projection.NbPoints() > 0 && projection.LowerDistance() < 1e-6)
            pieces.push_back(candidate);
    }
    return pieces;
}

// M05/M07: all edges with one value.
template <class Builder>
ShapeOperationResult treatEdges(KernelOperation operation, BooleanFailure failure,
                                const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                                double value) {
    const std::vector<double> values(edges.size(), value);
    return treatEdgesEach<Builder>(operation, failure, input, edges, values);
}

// User report 2026-10-13: OCCT refuses several edges at once where their corners meet other
// treatments (a loop around a chamfered corner), although each edge alone works. The edges
// are then treated one after the other in the same feature; the histories are merged so
// names follow through all steps.
template <class Builder>
ShapeOperationResult treatEdgesOneByOne(KernelOperation operation, BooleanFailure failure,
                                        const TopoDS_Shape& input,
                                        std::span<const TopoDS_Edge> edges, double value) {
    TopoDS_Shape current = input;
    Handle(BRepTools_History) history;
    try {
        for (const TopoDS_Edge& original : edges) {
            // Earlier steps shorten neighbouring edges into new shapes; find them on the curve.
            const auto pieces =
                history.IsNull() ? std::vector<TopoDS_Edge>{original} : piecesOf(current, original);
            if (pieces.empty())
                throw KernelError(operation, "an edge vanished in an earlier step", failure);
            Builder builder(current);
            for (const TopoDS_Edge& piece : pieces)
                builder.Add(value, piece);
            builder.Build();
            if (!builder.IsDone() || builder.Shape().IsNull() ||
                solidCountOf(builder.Shape()) != solidCountOf(input) ||
                !detail::isValidShape(builder.Shape()))
                throw KernelError(operation, "the edges cannot take this radius or distance",
                                  failure);
            NCollection_List<TopoDS_Shape> arguments;
            arguments.Append(current);
            Handle(BRepTools_History) step = new BRepTools_History(arguments, builder);
            if (history.IsNull())
                history = step;
            else
                history->Merge(step);
            current = builder.Shape();
        }
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        throw KernelError(operation, occtFailureDiagnostic(error), failure);
    }
    ShapeOperationResult result{current, {}, {}};
    detail::MergedHistory merged{*history};
    detail::appendHistory(result, merged, true, input, HistoryOperand::first);
    return result;
}
// User report 2026-10-13 (third): a loop of chosen edges broken by a short unchosen edge at a
// treated corner (the bottom edge of a chamfer) cannot be rounded beyond a small radius; the
// loop is closed by that edge, as CAD systems do for an edge chain. Bridges are unchosen edges
// whose two ends each meet a chosen edge on a face the three share.
std::vector<TopoDS_Edge> withBridges(const TopoDS_Shape& input,
                                     std::span<const TopoDS_Edge> edges) {
    AncestorMap edgesAt, facesOf;
    TopExp::MapShapesAndAncestors(input, TopAbs_VERTEX, TopAbs_EDGE, edgesAt);
    TopExp::MapShapesAndAncestors(input, TopAbs_EDGE, TopAbs_FACE, facesOf);
    const auto chosen = [&](const TopoDS_Shape& edge) {
        return std::ranges::any_of(edges, [&](const auto& item) { return item.IsSame(edge); });
    };
    const auto sharesFace = [&](const TopoDS_Shape& a, const TopoDS_Shape& b,
                                const TopoDS_Shape& face) {
        const auto onFace = [&](const TopoDS_Shape& edge) {
            if (!facesOf.Contains(edge))
                return false;
            for (const auto& candidate : facesOf.FindFromKey(edge))
                if (candidate.IsSame(face))
                    return true;
            return false;
        };
        return onFace(a) && onFace(b);
    };
    std::vector<TopoDS_Edge> result(edges.begin(), edges.end());
    for (TopExp_Explorer explorer(input, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge candidate = TopoDS::Edge(explorer.Current());
        if (chosen(candidate) || BRep_Tool::Degenerated(candidate) ||
            std::ranges::any_of(result, [&](const auto& item) { return item.IsSame(candidate); }) ||
            !facesOf.Contains(candidate))
            continue;
        TopoDS_Vertex first, last;
        TopExp::Vertices(candidate, first, last);
        if (first.IsNull() || last.IsNull() || first.IsSame(last))
            continue;
        bool bridges = false;
        for (const auto& face : facesOf.FindFromKey(candidate)) {
            const auto chosenAt = [&](const TopoDS_Vertex& vertex) {
                if (!edgesAt.Contains(vertex))
                    return false;
                for (const auto& neighbour : edgesAt.FindFromKey(vertex))
                    if (!neighbour.IsSame(candidate) && chosen(neighbour) &&
                        sharesFace(neighbour, candidate, face))
                        return true;
                return false;
            };
            if (chosenAt(first) && chosenAt(last))
                bridges = true;
        }
        if (bridges)
            result.push_back(candidate);
    }
    return result;
}

// All edges in one step, else one after the other, else with the loop closed by bridges.
template <class Builder>
ShapeOperationResult treatEdgesRobustly(KernelOperation operation, BooleanFailure failure,
                                        const TopoDS_Shape& input,
                                        std::span<const TopoDS_Edge> edges, double value) {
    try {
        return treatEdges<Builder>(operation, failure, input, edges, value);
    } catch (const KernelError& together) {
        if (together.failure() != failure || edges.size() < 2)
            throw;
        try {
            return treatEdgesOneByOne<Builder>(operation, failure, input, edges, value);
        } catch (const KernelError&) {
        }
        const auto bridged = withBridges(input, edges);
        if (bridged.size() == edges.size())
            throw together;
        try {
            return treatEdges<Builder>(operation, failure, input, bridged, value);
        } catch (const KernelError&) {
        }
        try {
            return treatEdgesOneByOne<Builder>(operation, failure, input, bridged, value);
        } catch (const KernelError&) {
            throw together;
        }
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
    return treatEdgesRobustly<BRepFilletAPI_MakeFillet>(
        KernelOperation::fillet, BooleanFailure::filletFailed, input, edges, radius);
}

ShapeOperationResult ShapeOperations::filletEachWithHistory(const TopoDS_Shape& input,
                                                            std::span<const TopoDS_Edge> edges,
                                                            std::span<const double> radii) {
    return treatEdgesEach<BRepFilletAPI_MakeFillet>(
        KernelOperation::fillet, BooleanFailure::filletFailed, input, edges, radii);
}

ShapeOperationResult ShapeOperations::chamferEachWithHistory(const TopoDS_Shape& input,
                                                             std::span<const TopoDS_Edge> edges,
                                                             std::span<const double> distances) {
    return treatEdgesEach<BRepFilletAPI_MakeChamfer>(
        KernelOperation::chamfer, BooleanFailure::chamferFailed, input, edges, distances);
}

std::vector<std::size_t> ShapeOperations::failingFilletEdges(const TopoDS_Shape& input,
                                                             std::span<const TopoDS_Edge> edges,
                                                             double radius) {
    return failingEdges(edges, [&](auto one) { return filletWithHistory(input, one, radius); });
}

namespace {
// User report 2026-10-13: OCCT's chamfer builder refuses an edge that runs into a rounding
// smaller than the chamfer. A straight edge between two planar faces is then chamfered by
// cutting a wedge with legs `distance` along both faces; at an end on a curved face (a
// rounding) the wedge goes on by `distance`, so the chamfer passes through the rounding.
std::optional<TopoDS_Shape> chamferWedge(const TopoDS_Edge& edge, double distance,
                                         const AncestorMap& faces, const AncestorMap& corners) {
    const BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line || !faces.Contains(edge))
        return std::nullopt;
    gp_Pnt start = curve.Value(curve.FirstParameter());
    gp_Pnt end = curve.Value(curve.LastParameter());
    gp_Vec along(start, end);
    if (along.Magnitude() < 1e-9)
        return std::nullopt;
    along.Normalize();
    std::vector<gp_Vec> legs;
    for (const auto& adjacent : faces.FindFromKey(edge)) {
        const TopoDS_Face face = TopoDS::Face(adjacent);
        const BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane)
            return std::nullopt;
        const gp_Vec outward(planeNormal(surface.Plane(), face.Orientation()));
        gp_Vec leg = outward.Crossed(along);
        const gp_Pnt middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        const gp_Pnt probe = middle.Translated(leg * (distance * 0.01));
        if (BRepClass_FaceClassifier(face, probe, 1e-7).State() != TopAbs_IN)
            leg.Reverse();
        leg.Normalize();
        // The chamfer must end on this face; a larger distance is refused, as by the builder.
        const auto reach =
            BRepClass_FaceClassifier(face, middle.Translated(leg * distance), 1e-7).State();
        if (reach != TopAbs_IN && reach != TopAbs_ON)
            return std::nullopt;
        legs.push_back(leg);
    }
    if (legs.size() != 2)
        return std::nullopt;
    const auto onCurvedFace = [&](const TopoDS_Shape& vertex) {
        if (!corners.Contains(vertex))
            return false;
        for (const auto& face : corners.FindFromKey(vertex))
            if (BRepAdaptor_Surface(TopoDS::Face(face)).GetType() != GeomAbs_Plane)
                return true;
        return false;
    };
    TopoDS_Vertex first, last;
    TopExp::Vertices(edge, first, last, true);
    const gp_Pnt firstPoint = BRep_Tool::Pnt(first);
    const bool startIsFirst = firstPoint.Distance(start) < firstPoint.Distance(end);
    if (onCurvedFace(startIsFirst ? first : last))
        start.Translate(-along * distance);
    if (onCurvedFace(startIsFirst ? last : first))
        end.Translate(along * distance);
    BRepBuilderAPI_MakePolygon triangle(start, start.Translated(legs[0] * distance),
                                        start.Translated(legs[1] * distance), true);
    if (!triangle.IsDone())
        return std::nullopt;
    BRepBuilderAPI_MakeFace profile(triangle.Wire(), true);
    if (!profile.IsDone())
        return std::nullopt;
    BRepPrimAPI_MakePrism wedge(profile.Face(), gp_Vec(start, end));
    if (!wedge.IsDone())
        return std::nullopt;
    return wedge.Shape();
}

ShapeOperationResult chamferByCut(const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                                  double distance) {
    AncestorMap faces, corners;
    TopExp::MapShapesAndAncestors(input, TopAbs_EDGE, TopAbs_FACE, faces);
    TopExp::MapShapesAndAncestors(input, TopAbs_VERTEX, TopAbs_FACE, corners);
    std::vector<TopoDS_Shape> wedges;
    for (const TopoDS_Edge& edge : edges) {
        const auto wedge = chamferWedge(edge, distance, faces, corners);
        if (!wedge)
            throw KernelError(KernelOperation::chamfer,
                              "the edge is no straight edge between two planar faces",
                              BooleanFailure::chamferFailed);
        wedges.push_back(*wedge);
    }
    const TopoDS_Shape tool =
        wedges.size() == 1 ? wedges.front() : ShapeOperations::compound(wedges);
    try {
        auto result = ShapeOperations::cutWithHistory(input, tool);
        if (solidCountOf(result.shape) != solidCountOf(input))
            throw KernelError(KernelOperation::chamfer, "the chamfer would split the body",
                              BooleanFailure::chamferFailed);
        return result;
    } catch (const KernelError& error) {
        if (error.operation() == KernelOperation::chamfer)
            throw;
        throw KernelError(KernelOperation::chamfer, error.what(), BooleanFailure::chamferFailed);
    }
}
} // namespace

ShapeOperationResult ShapeOperations::chamferWithHistory(const TopoDS_Shape& input,
                                                         std::span<const TopoDS_Edge> edges,
                                                         double distance) {
    try {
        return treatEdgesRobustly<BRepFilletAPI_MakeChamfer>(
            KernelOperation::chamfer, BooleanFailure::chamferFailed, input, edges, distance);
    } catch (const KernelError& builderError) {
        if (builderError.failure() != BooleanFailure::chamferFailed)
            throw;
        try {
            return chamferByCut(input, edges, distance);
        } catch (const KernelError&) {
            throw builderError; // the builder's reason stays the one reported
        }
    }
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
        throw KernelError(
            KernelOperation::cut, std::string("overlap validation failed: ") + error.what(),
            error.failure() == BooleanFailure::emptyIntersection ? BooleanFailure::noMaterialRemoved
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
