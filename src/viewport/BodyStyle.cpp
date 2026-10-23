// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "BodyStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_TypeOfHLR.hxx>
#include <Quantity_TypeOfColor.hxx>

namespace voelund::viewport {

Quantity_Color rgbColor(std::uint32_t color) {
    const auto channel = [color](int shift) { return ((color >> shift) & 0xFFU) / 255.0; };
    return {channel(16), channel(8), channel(0), Quantity_TOC_sRGB};
}

int bodyDisplayMode(DisplayStyle style) noexcept {
    return style == DisplayStyle::wireframe || style == DisplayStyle::hiddenEdges ? AIS_WireFrame
                                                                                  : AIS_Shaded;
}

void styleBody(const occ::handle<AIS_Shape>& shape, DisplayStyle style,
               const Quantity_Color& edge) {
    const auto& drawer = shape->Attributes();
    drawer->SetFaceBoundaryDraw(style == DisplayStyle::shadedWithEdges);
    drawer->SetFaceBoundaryAspect(new Prs3d_LineAspect(edge, Aspect_TOL_SOLID, 1.0));
    if (bodyDisplayMode(style) == AIS_WireFrame) {
        // The lines of a wireframe in the edge colour, so they read on any background.
        for (const auto setter :
             {&Prs3d_Drawer::SetWireAspect, &Prs3d_Drawer::SetFreeBoundaryAspect,
              &Prs3d_Drawer::SetUnFreeBoundaryAspect, &Prs3d_Drawer::SetSeenLineAspect})
            ((*drawer).*setter)(new Prs3d_LineAspect(edge, Aspect_TOL_SOLID, 1.0));
        drawer->SetHiddenLineAspect(new Prs3d_LineAspect(edge, Aspect_TOL_DASH, 1.0));
        drawer->SetTypeOfHLR(Prs3d_TOH_PolyAlgo);
    }
    if (style == DisplayStyle::hiddenEdges)
        drawer->EnableDrawHiddenLine();
    else
        drawer->DisableDrawHiddenLine();
    shape->SetDisplayMode(bodyDisplayMode(style));
}

} // namespace voelund::viewport
