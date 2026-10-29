// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OcctGuard.hpp"
#include <voelund/kernel/PlaneSection.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRep_Tool.hxx>
#include <GeomConvert_ApproxCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

namespace voelund::kernel {
namespace {
constexpr auto operation = KernelOperation::sketchGeometry;
} // namespace

std::vector<TopoDS_Edge> planeSectionEdges(const TopoDS_Shape& body, const gp_Pln& plane) {
    return detail::guarded(operation, [&] {
        BRepAlgoAPI_Section section(body, plane, false);
        section.ComputePCurveOn1(false);
        section.Approximation(false);
        section.Build();
        std::vector<TopoDS_Edge> edges;
        if (!section.IsDone())
            return edges;
        for (TopExp_Explorer edge(section.Shape(), TopAbs_EDGE); edge.More(); edge.Next())
            edges.push_back(TopoDS::Edge(edge.Current()));
        return edges;
    });
}

std::optional<EdgeEllipse> edgeEllipse(const TopoDS_Edge& edge) {
    return detail::guarded(operation, [&]() -> std::optional<EdgeEllipse> {
        double first = 0.0, last = 0.0;
        Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        if (const auto trimmed = Handle(Geom_TrimmedCurve)::DownCast(curve); !trimmed.IsNull())
            curve = trimmed->BasisCurve();
        const auto ellipse = Handle(Geom_Ellipse)::DownCast(curve);
        if (ellipse.IsNull())
            return std::nullopt;
        return EdgeEllipse{ellipse->Elips(), first, last};
    });
}

std::optional<SpaceSpline> cubicSplineOfEdge(const TopoDS_Edge& edge, double tolerance,
                                             int maximumPoles) {
    return detail::guarded(operation, [&]() -> std::optional<SpaceSpline> {
        const Handle(BRepAdaptor_Curve) adaptor = new BRepAdaptor_Curve(edge);
        GeomConvert_ApproxCurve approximation(adaptor, tolerance, GeomAbs_C2, maximumPoles - 3, 3);
        if (!approximation.HasResult())
            return std::nullopt;
        const Handle(Geom_BSplineCurve) spline = approximation.Curve();
        if (spline->Degree() < 3)
            spline->IncreaseDegree(3);
        if (spline->IsPeriodic())
            spline->SetNotPeriodic();
        if (spline->Degree() != 3 || spline->NbPoles() > maximumPoles || spline->IsRational())
            return std::nullopt;
        SpaceSpline result;
        for (int index = 1; index <= spline->NbPoles(); ++index)
            result.poles.push_back(spline->Pole(index));
        for (int index = 1; index <= spline->NbKnots(); ++index)
            result.knots.insert(result.knots.end(),
                                static_cast<std::size_t>(spline->Multiplicity(index)),
                                spline->Knot(index) - spline->Knot(1));
        return result;
    });
}

} // namespace voelund::kernel
