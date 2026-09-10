#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>

namespace voelund::viewport {
namespace {

void setHighlightColor(
    const occ::handle<AIS_InteractiveContext>& context,
    Prs3d_TypeOfHighlight type,
    const Quantity_Color& color,
    float transparency)
{
    const occ::handle<Prs3d_Drawer>& style = context->HighlightStyle(type);
    style->SetColor(color);
    style->SetTransparency(transparency);
    style->SetDisplayMode(AIS_Shaded);
}

} // namespace

void configureHighlightStyles(const occ::handle<AIS_InteractiveContext>& context)
{
    const Quantity_Color hoverColor(1.0, 0.58, 0.08, Quantity_TOC_RGB);
    const Quantity_Color selectedColor(0.05, 0.42, 1.0, Quantity_TOC_RGB);

    setHighlightColor(context, Prs3d_TypeOfHighlight_Dynamic, hoverColor, 0.12F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_LocalDynamic, hoverColor, 0.12F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_Selected, selectedColor, 0.0F);
    setHighlightColor(context, Prs3d_TypeOfHighlight_LocalSelected, selectedColor, 0.0F);
}

} // namespace voelund::viewport
