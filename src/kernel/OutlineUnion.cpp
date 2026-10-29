// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OcctGuard.hpp"
#include <voelund/kernel/OutlineUnion.hpp>

#include <BOPTools_AlgoTools3D.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Tool.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Line.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IntTools_Context.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pln.hxx>

#include <algorithm>

namespace voelund::kernel {
namespace {
constexpr auto operation = KernelOperation::sketchGeometry;

gp_Pnt lifted(const gp_Pnt2d& point) { return {point.X(), point.Y(), 0.0}; }

TopoDS_Edge segmentEdge(const OutlineSegment& segment) {
    if (segment.straight)
        return BRepBuilderAPI_MakeEdge(lifted(segment.points[0]), lifted(segment.points[1]));
    NCollection_Array1<gp_Pnt> poles(1, 4);
    for (int index = 0; index < 4; ++index)
        poles.SetValue(index + 1, lifted(segment.points[static_cast<std::size_t>(index)]));
    return BRepBuilderAPI_MakeEdge(Handle(Geom_BezierCurve)(new Geom_BezierCurve(poles)));
}

// A rectangle around all contour points, with a margin.
TopoDS_Face enclosing(const std::vector<OutlineContour>& contours) {
    double left = 1e300, right = -1e300, bottom = 1e300, top = -1e300;
    for (const auto& contour : contours)
        for (const auto& segment : contour)
            for (const auto& point : segment.points) {
                left = std::min(left, point.X());
                right = std::max(right, point.X());
                bottom = std::min(bottom, point.Y());
                top = std::max(top, point.Y());
            }
    const double margin = 1.0 + 0.1 * std::max(right - left, top - bottom);
    return BRepBuilderAPI_MakeFace(gp_Pln(gp::XOY()), left - margin, right + margin,
                                   bottom - margin, top + margin);
}

// The cells the contours cut the rectangle into, those `filled` keeps.
NCollection_List<TopoDS_Shape> filledCells(const std::vector<OutlineContour>& contours,
                                           const std::function<bool(const gp_Pnt2d&)>& filled) {
    NCollection_List<TopoDS_Shape> objects, tools, cells;
    objects.Append(enclosing(contours));
    for (const auto& contour : contours)
        for (const auto& segment : contour)
            tools.Append(segmentEdge(segment));
    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(objects);
    splitter.SetTools(tools);
    splitter.Build();
    if (!splitter.IsDone())
        throw KernelError(operation, "Outline contours do not split the plane");
    const Handle(IntTools_Context) context = new IntTools_Context;
    for (TopExp_Explorer face(splitter.Shape(), TopAbs_FACE); face.More(); face.Next()) {
        gp_Pnt inside;
        gp_Pnt2d onSurface;
        if (BOPTools_AlgoTools3D::PointInFace(TopoDS::Face(face.Current()), inside, onSurface,
                                              context) == 0 &&
            filled({inside.X(), inside.Y()}))
            cells.Append(face.Current());
    }
    return cells;
}

TopoDS_Shape merged(NCollection_List<TopoDS_Shape> cells) {
    TopoDS_Shape area = cells.First();
    if (cells.Size() > 1) {
        NCollection_List<TopoDS_Shape> first;
        first.Append(cells.First());
        cells.RemoveFirst();
        BRepAlgoAPI_Fuse fuse;
        fuse.SetArguments(first);
        fuse.SetTools(cells);
        fuse.Build();
        area = fuse.Shape();
    }
    ShapeUpgrade_UnifySameDomain unify(area, true, true, false);
    unify.Build();
    return unify.Shape();
}

OutlinePiece pieceOf(const TopoDS_Edge& edge) {
    TopoDS_Vertex first, last;
    TopExp::Vertices(edge, first, last);
    const gp_Pnt a = BRep_Tool::Pnt(first), b = BRep_Tool::Pnt(last);
    double from = 0.0, to = 0.0;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, from, to);
    if (const auto trimmed = Handle(Geom_TrimmedCurve)::DownCast(curve); !trimmed.IsNull())
        curve = trimmed->BasisCurve();
    OutlinePiece piece{true, {a.X(), a.Y()}, {b.X(), b.Y()}, {}};
    if (!Handle(Geom_Line)::DownCast(curve).IsNull())
        return piece;
    Handle(Geom_BSplineCurve) spline =
        GeomConvert::CurveToBSplineCurve(new Geom_TrimmedCurve(curve, from, to));
    if (spline->Degree() < 3)
        spline->IncreaseDegree(3);
    if (spline->Degree() != 3 || spline->IsRational())
        throw KernelError(operation, "Outline piece is no cubic B-spline");
    piece.straight = false;
    for (int index = 1; index <= spline->NbPoles(); ++index)
        piece.spline.poles.emplace_back(spline->Pole(index).X(), spline->Pole(index).Y());
    for (int index = 1; index <= spline->NbKnots(); ++index)
        piece.spline.knots.insert(piece.spline.knots.end(),
                                  static_cast<std::size_t>(spline->Multiplicity(index)),
                                  spline->Knot(index) - spline->Knot(1));
    return piece;
}

} // namespace

std::vector<OutlinePiece> mergedOutline(const std::vector<OutlineContour>& contours,
                                        const std::function<bool(const gp_Pnt2d&)>& filled) {
    if (contours.empty())
        return {};
    return detail::guarded(operation, [&] {
        const auto cells = filledCells(contours, filled);
        if (cells.IsEmpty())
            return std::vector<OutlinePiece>{};
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(merged(cells), TopAbs_EDGE, edges);
        std::vector<OutlinePiece> pieces;
        for (int index = 1; index <= edges.Extent(); ++index)
            pieces.push_back(pieceOf(TopoDS::Edge(edges(index))));
        return pieces;
    });
}

} // namespace voelund::kernel
