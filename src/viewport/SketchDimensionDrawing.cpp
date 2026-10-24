// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchDimensionDrawing.hpp"

#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_Presentation.hxx>
#include <SelectMgr_Selection.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace voelund::viewport {
namespace {

gp_Pnt along(const gp_Pnt& from, const gp_Dir& direction, double distance) {
    return from.Translated(gp_Vec(direction) * distance);
}

double component(const gp_Pnt& point, const gp_Pnt& origin, const gp_Dir& direction) {
    return gp_Vec(origin, point).Dot(gp_Vec(direction));
}

// Linear: extension lines from both points past the dimension line, which stands `offset`
// beyond the farther point; arrows at both ends pointing out to the extension lines.
DimensionDrawing linear(const SceneDimension& d) {
    DimensionDrawing drawing;
    const double level =
        std::max(0.0, component(d.second, d.first, d.outward)) + d.offset; // from `first`
    const gp_Pnt a = along(d.first, d.outward, level);
    const gp_Pnt b = along(d.second, d.outward, level - component(d.second, d.first, d.outward));
    const double gap = 0.2 * d.offset;
    const double beyond = 0.25 * d.offset;
    drawing.segments.push_back({along(d.first, d.outward, gap), along(a, d.outward, beyond)});
    const double secondStart = std::min(gap, level - component(d.second, d.first, d.outward));
    drawing.segments.push_back(
        {along(d.second, d.outward, secondStart), along(b, d.outward, beyond)});
    drawing.segments.push_back({a, b});
    const double length = a.Distance(b);
    drawing.arrowLength = std::min(0.18 * d.offset, 0.25 * length);
    if (length > 1e-9) {
        const gp_Dir ab(gp_Vec(a, b));
        drawing.arrows.push_back({a, ab.Reversed()});
        drawing.arrows.push_back({b, ab});
    }
    drawing.text = gp_Pnt((a.XYZ() + b.XYZ()) / 2);
    return drawing;
}

// Radius: from the centre to the curve, the arrow on the curve. Diameter: across, an arrow at
// each end. The value sits on the line, nearer the curve.
DimensionDrawing radial(const SceneDimension& d) {
    DimensionDrawing drawing;
    drawing.segments.push_back({d.first, d.second});
    const double length = d.first.Distance(d.second);
    drawing.arrowLength = std::min(0.18 * d.offset, 0.25 * length);
    if (length > 1e-9) {
        const gp_Dir out(gp_Vec(d.first, d.second));
        drawing.arrows.push_back({d.second, out});
        if (d.style == SceneDimensionStyle::diameter)
            drawing.arrows.push_back({d.first, out.Reversed()});
    }
    drawing.text =
        gp_Pnt((d.style == SceneDimensionStyle::diameter ? d.vertex.XYZ() + d.second.XYZ()
                                                         : d.first.XYZ() + d.second.XYZ()) /
               2);
    return drawing;
}

// Angle: an arc about the vertex between the legs, arrows at both ends along the arc, and the
// legs drawn out to the arc.
DimensionDrawing angular(const SceneDimension& d) {
    DimensionDrawing drawing;
    const gp_Vec first(d.vertex, d.first);
    const gp_Vec second(d.vertex, d.second);
    const double radius = first.Magnitude();
    if (radius < 1e-9 || second.Magnitude() < 1e-9)
        return drawing;
    const gp_Dir u(first);
    const gp_Dir v = d.normal.Crossed(u);
    double sweep = std::atan2(second.Dot(gp_Vec(v)), second.Dot(gp_Vec(u)));
    const int steps = 24;
    gp_Pnt previous = d.first;
    for (int step = 1; step <= steps; ++step) {
        const double angle = sweep * step / steps;
        const gp_Pnt next = d.vertex.Translated(gp_Vec(u) * (radius * std::cos(angle)) +
                                                gp_Vec(v) * (radius * std::sin(angle)));
        drawing.segments.push_back({previous, next});
        previous = next;
    }
    drawing.arrowLength = std::min(0.07 * radius, 0.25 * radius * std::abs(sweep));
    const double sign = sweep < 0 ? -1.0 : 1.0;
    const gp_Dir startTangent(gp_Vec(v) * -sign);
    const gp_Dir endTangent(gp_Vec(d.normal.Crossed(gp_Dir(second))) * sign);
    drawing.arrows.push_back({d.first, startTangent});
    drawing.arrows.push_back({along(d.vertex, gp_Dir(second), radius), endTangent});
    const double half = sweep / 2;
    drawing.text = d.vertex.Translated(gp_Vec(u) * (radius * std::cos(half)) +
                                       gp_Vec(v) * (radius * std::sin(half)));
    return drawing;
}

class DimensionGraphic final : public AIS_InteractiveObject {
  public:
    DimensionGraphic(DimensionDrawing drawing, const gp_Dir& normal, const Quantity_Color& color)
        : drawing_(std::move(drawing)), normal_(normal), color_(color) {}

  protected:
    void Compute(const occ::handle<PrsMgr_PresentationManager>&,
                 const occ::handle<Prs3d_Presentation>& presentation, const int mode) override {
        if (mode != 0)
            return;
        const occ::handle<Graphic3d_Group> group = presentation->NewGroup();
        group->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(color_, Aspect_TOL_SOLID, 1.0));
        occ::handle<Graphic3d_ArrayOfSegments> lines =
            new Graphic3d_ArrayOfSegments(static_cast<int>(2 * drawing_.segments.size()));
        for (const auto& [from, to] : drawing_.segments) {
            lines->AddVertex(from);
            lines->AddVertex(to);
        }
        group->AddPrimitiveArray(lines);
        if (drawing_.arrows.empty() || drawing_.arrowLength <= 0.0)
            return;
        occ::handle<Graphic3d_AspectFillArea3d> fill = new Graphic3d_AspectFillArea3d();
        fill->SetInteriorStyle(Aspect_IS_SOLID);
        fill->SetInteriorColor(color_);
        fill->SetShadingModel(Graphic3d_TypeOfShadingModel_Unlit);
        fill->SetFaceCulling(Graphic3d_TypeOfBackfacingModel_DoubleSided);
        // As in Fusion, arrowheads keep their size on screen at every zoom: each is drawn in
        // pixels about its tip (zoom persistent), unless the dimension line is too short.
        constexpr double pixels = 11.0;
        for (const auto& arrow : drawing_.arrows) {
            const occ::handle<Graphic3d_Group> head = presentation->NewGroup();
            head->SetTransformPersistence(
                new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, arrow.tip));
            head->SetGroupPrimitivesAspect(fill);
            occ::handle<Graphic3d_ArrayOfTriangles> triangle = new Graphic3d_ArrayOfTriangles(3);
            const gp_Pnt tip(0, 0, 0);
            const gp_Pnt base = along(tip, arrow.direction, -pixels);
            const gp_Dir side = normal_.Crossed(arrow.direction);
            triangle->AddVertex(tip);
            triangle->AddVertex(along(base, side, 0.32 * pixels));
            triangle->AddVertex(along(base, side, -0.32 * pixels));
            head->AddPrimitiveArray(triangle);
        }
    }
    void ComputeSelection(const occ::handle<SelectMgr_Selection>&, const int) override {}

  private:
    DimensionDrawing drawing_;
    gp_Dir normal_;
    Quantity_Color color_;
};

} // namespace

DimensionDrawing drawDimension(const SceneDimension& dimension) {
    switch (dimension.style) {
    case SceneDimensionStyle::linear:
        return linear(dimension);
    case SceneDimensionStyle::radius:
    case SceneDimensionStyle::diameter:
        return radial(dimension);
    case SceneDimensionStyle::angle:
        return angular(dimension);
    }
    return {};
}

occ::handle<AIS_InteractiveObject> makeDimensionGraphic(const DimensionDrawing& drawing,
                                                        const gp_Dir& normal,
                                                        const Quantity_Color& color) {
    return new DimensionGraphic(drawing, normal, color);
}

} // namespace voelund::viewport
