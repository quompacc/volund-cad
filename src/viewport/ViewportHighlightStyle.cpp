// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>

namespace voelund::viewport {
namespace {

void setHighlightColor(const occ::handle<AIS_InteractiveContext>& context,
                       Prs3d_TypeOfHighlight type, const Quantity_Color& color,
                       float transparency) {
    const occ::handle<Prs3d_Drawer>& style = context->HighlightStyle(type);
    style->SetColor(color);
    style->SetTransparency(transparency);
    style->SetDisplayMode(AIS_Shaded);
    // S14: in the object's own layer, so a highlighted face stays behind what is in front of
    // it (OCCT draws hover highlights over everything by default).
    style->SetZLayer(Graphic3d_ZLayerId_UNKNOWN);
    // Matte, so a face lit head-on keeps the highlight color instead of washing out to white
    // on the light theme (U03).
    style->SetupOwnShadingAspect();
    style->ShadingAspect()->SetMaterial(Graphic3d_NameOfMaterial_Satin);
    style->ShadingAspect()->SetColor(color);
    style->ShadingAspect()->SetTransparency(transparency);
}

} // namespace

Quantity_Color occtColor(const QColor& color) {
    return Quantity_Color(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_sRGB);
}

void configureHighlightStyles(const occ::handle<AIS_InteractiveContext>& context,
                              const ViewportTheme& theme) {
    const Quantity_Color hoverColor = occtColor(theme.hover);
    const Quantity_Color selectedColor = occtColor(theme.selection);

    setHighlightColor(context, Prs3d_TypeOfHighlight_Dynamic, hoverColor, 0.12F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_LocalDynamic, hoverColor, 0.12F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_Selected, selectedColor, 0.0F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_LocalSelected, selectedColor, 0.0F);
}

} // namespace voelund::viewport
