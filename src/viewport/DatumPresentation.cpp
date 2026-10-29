// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DatumPresentation.hpp"
#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Builder.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

namespace voelund::viewport {
namespace {
// The construction color: amber, apart from the axis colors of the base planes.
const Quantity_Color& constructionColor() {
    static const Quantity_Color color = occtColor(QColor(0xC9, 0x8A, 0x1B));
    return color;
}
// N13b: a construction point as a small three-armed cross.
TopoDS_Shape pointCross(const gp_Pnt& at, double arm) {
    TopoDS_Compound cross;
    BRep_Builder builder;
    builder.MakeCompound(cross);
    for (const gp_Vec direction : {gp_Vec(1, 0, 0), gp_Vec(0, 1, 0), gp_Vec(0, 0, 1)})
        builder.Add(cross, BRepBuilderAPI_MakeEdge(at.Translated(direction * -arm),
                                                   at.Translated(direction * arm))
                               .Edge());
    return cross;
}
} // namespace

void DatumPresentation::show(const occ::handle<AIS_InteractiveContext>& context,
                             std::span<const SceneDatum> datums, double halfSize,
                             const ViewportTheme& theme) {
    hide(context);
    for (const SceneDatum& datum : datums) {
        const gp_Pnt origin(datum.origin[0], datum.origin[1], datum.origin[2]);
        const gp_Dir normal(datum.normal[0], datum.normal[1], datum.normal[2]);
        TopoDS_Shape shape;
        if (datum.point) {
            shape = pointCross(origin, halfSize * 0.06);
        } else if (datum.axis) {
            shape = BRepBuilderAPI_MakeEdge(origin.Translated(gp_Vec(normal) * -halfSize),
                                            origin.Translated(gp_Vec(normal) * halfSize))
                        .Edge();
        } else {
            const gp_Dir x(datum.xDirection[0], datum.xDirection[1], datum.xDirection[2]);
            const double half = halfSize * 0.6;
            shape =
                BRepBuilderAPI_MakeFace(gp_Pln(gp_Ax3(origin, normal, x)), -half, half, -half, half)
                    .Face();
        }
        occ::handle<AIS_Shape> presentation = new AIS_Shape(shape);
        presentation->SetColor(constructionColor());
        const bool line = datum.axis || datum.point;
        presentation->SetTransparency(line ? 0.0 : 0.85);
        presentation->SetWidth(line ? 2.0 : 1.0);
        presentation->Attributes()->SetFaceBoundaryDraw(true);
        presentation->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(constructionColor(), Aspect_TOL_DASH, 1.2));
        const auto highlight = [&](const Quantity_Color& color, float transparency) {
            occ::handle<Prs3d_Drawer> drawer = new Prs3d_Drawer();
            drawer->SetColor(color);
            drawer->SetTransparency(transparency);
            drawer->SetDisplayMode(AIS_Shaded);
            drawer->SetZLayer(Graphic3d_ZLayerId_Default);
            return drawer;
        };
        presentation->SetDynamicHilightAttributes(highlight(occtColor(theme.hover), 0.55F));
        presentation->SetHilightAttributes(highlight(occtColor(theme.selection), 0.45F));
        context->Display(presentation, AIS_Shaded,
                         AIS_Shape::SelectionMode(line ? TopAbs_EDGE : TopAbs_FACE), false);
        shapes_.emplace_back(datum.id, presentation);
    }
}

void DatumPresentation::setPickable(const occ::handle<AIS_InteractiveContext>& context,
                                    bool pickable) {
    for (const auto& [id, shape] : shapes_) {
        context->Deactivate(shape);
        if (pickable)
            context->Activate(
                shape, AIS_Shape::SelectionMode(
                           shape->Shape().ShapeType() == TopAbs_EDGE ? TopAbs_EDGE : TopAbs_FACE));
    }
}

void DatumPresentation::hide(const occ::handle<AIS_InteractiveContext>& context) {
    for (auto& [id, shape] : shapes_)
        context->Remove(shape, false);
    shapes_.clear();
}

std::optional<std::uint64_t>
DatumPresentation::datum(const occ::handle<AIS_InteractiveObject>& object) const {
    for (const auto& [id, shape] : shapes_)
        if (shape == object)
            return id;
    return std::nullopt;
}

} // namespace voelund::viewport
