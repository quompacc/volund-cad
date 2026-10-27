// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/EdgeQueries.hpp>

#include "OcctGuard.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <deque>
#include <numbers>

namespace voelund::kernel {
namespace {

// The edge's direction where it passes the vertex.
gp_Vec tangentAt(const TopoDS_Edge& edge, const TopoDS_Vertex& vertex) {
    const BRepAdaptor_Curve curve(edge);
    gp_Pnt point;
    gp_Vec tangent;
    curve.D1(BRep_Tool::Parameter(vertex, edge), point, tangent);
    return tangent;
}

// Smooth at the vertex: both edges run along one line there (either way), within 1 degree.
bool smooth(const TopoDS_Edge& first, const TopoDS_Edge& second, const TopoDS_Vertex& vertex) {
    const gp_Vec a = tangentAt(first, vertex);
    const gp_Vec b = tangentAt(second, vertex);
    const double lengths = a.Magnitude() * b.Magnitude();
    return lengths > 1e-18 &&
           a.Crossed(b).Magnitude() / lengths < std::sin(std::numbers::pi / 180.0);
}

using VertexEdges = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

gp_Pnt along(const TopoDS_Edge& edge, double share) {
    const BRepAdaptor_Curve curve(edge);
    return curve.Value(curve.FirstParameter() +
                       share * (curve.LastParameter() - curve.FirstParameter()));
}

// The body's own edge where `edge` lies: itself, or (from another copy of the geometry) the
// edge with the same ends and middle.
TopoDS_Edge ownEdge(const TopoDS_Shape& body, const TopoDS_Edge& edge) {
    const gp_Pnt middle = along(edge, 0.5);
    for (TopExp_Explorer explorer(body, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge candidate = TopoDS::Edge(explorer.Current());
        if (candidate.IsSame(edge))
            return candidate;
        if (along(candidate, 0.5).Distance(middle) < 1e-6 &&
            std::min(along(candidate, 0).Distance(along(edge, 0)) +
                         along(candidate, 1).Distance(along(edge, 1)),
                     along(candidate, 0).Distance(along(edge, 1)) +
                         along(candidate, 1).Distance(along(edge, 0))) < 1e-6)
            return candidate;
    }
    return edge;
}

// Adds the edges joined to `current` without a kink that the chain does not have yet.
void followSmooth(const VertexEdges& atVertex, const TopoDS_Edge& current,
                  std::vector<TopoDS_Shape>& chain, std::deque<TopoDS_Edge>& open) {
    const auto known = [&](const TopoDS_Shape& candidate) {
        return std::ranges::any_of(chain, [&](const auto& item) { return item.IsSame(candidate); });
    };
    for (TopExp_Explorer corner(current, TopAbs_VERTEX); corner.More(); corner.Next()) {
        const TopoDS_Vertex vertex = TopoDS::Vertex(corner.Current());
        if (!atVertex.Contains(vertex))
            continue;
        for (const auto& other : atVertex.FindFromKey(vertex)) {
            const TopoDS_Edge next = TopoDS::Edge(other);
            if (known(next) || BRep_Tool::Degenerated(next) || !smooth(current, next, vertex))
                continue;
            chain.push_back(next);
            open.push_back(next);
        }
    }
}

} // namespace

std::vector<TopoDS_Shape> tangentChain(const TopoDS_Shape& body, const TopoDS_Shape& edge) {
    return detail::guarded(KernelOperation::evaluation, [&] {
        std::vector<TopoDS_Shape> chain;
        if (edge.IsNull() || edge.ShapeType() != TopAbs_EDGE)
            return chain;
        VertexEdges atVertex;
        TopExp::MapShapesAndAncestors(body, TopAbs_VERTEX, TopAbs_EDGE, atVertex);
        const TopoDS_Edge start = ownEdge(body, TopoDS::Edge(edge));
        std::deque<TopoDS_Edge> open{start};
        chain.push_back(start);
        while (!open.empty()) {
            const TopoDS_Edge current = open.front();
            open.pop_front();
            followSmooth(atVertex, current, chain, open);
        }
        return chain;
    });
}

} // namespace voelund::kernel
