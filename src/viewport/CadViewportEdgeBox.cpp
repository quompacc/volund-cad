// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <array>

namespace voelund::viewport {
namespace {
constexpr int samples = 9;

// Points along the edge on screen (framebuffer pixels), ends included.
std::array<QPoint, samples> onScreen(const TopoDS_Edge& edge, const occ::handle<V3d_View>& view) {
    const BRepAdaptor_Curve curve(edge);
    std::array<QPoint, samples> points{};
    for (int index = 0; index < samples; ++index) {
        const double share = static_cast<double>(index) / (samples - 1);
        const gp_Pnt point = curve.Value(curve.FirstParameter() +
                                         share * (curve.LastParameter() - curve.FirstParameter()));
        int x = 0, y = 0;
        view->Convert(point.X(), point.Y(), point.Z(), x, y);
        points[static_cast<std::size_t>(index)] = QPoint(x, y);
    }
    return points;
}

bool boxed(const std::array<QPoint, samples>& points, QRect rectangle, bool crossing) {
    const auto inside = [&](QPoint point) { return rectangle.contains(point); };
    return crossing ? std::ranges::any_of(points, inside) : std::ranges::all_of(points, inside);
}
} // namespace

void CadViewport::setEdgeBoxSelectionHandler(
    std::function<void(std::uint64_t, const std::vector<TopoDS_Shape>&)> handler) {
    impl_->edgeBoxHandler = std::move(handler);
}

// W07 (as Fusion): a box drawn while a tool takes edges (fillet, chamfer) takes the body edges
// in it - left to right wholly inside, right to left touched (sampled along each edge).
void CadViewport::reportBoxedEdges(QRect rectangle, bool crossing) {
    if (!impl_->edgeBoxHandler)
        return;
    std::vector<std::pair<std::uint64_t, std::vector<TopoDS_Shape>>> found;
    for (const auto& presentation : impl_->presentations) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(presentation.shape->Shape(), TopAbs_EDGE, edges);
        std::vector<TopoDS_Shape> inBox;
        for (int index = 1; index <= edges.Extent(); ++index) {
            const TopoDS_Edge& edge = TopoDS::Edge(edges(index));
            if (!BRep_Tool::Degenerated(edge) &&
                boxed(onScreen(edge, impl_->view), rectangle, crossing))
                inBox.push_back(edge);
        }
        if (!inBox.empty())
            found.emplace_back(presentation.id, std::move(inBox));
    }
    // The handler may rebuild the presentations; the edges were collected first.
    const auto handler = impl_->edgeBoxHandler;
    for (const auto& [body, inBox] : found)
        handler(body, inBox);
}

} // namespace voelund::viewport
