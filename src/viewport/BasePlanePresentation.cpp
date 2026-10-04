#include "BasePlanePresentation.hpp"

#include <AIS_DisplayMode.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

namespace voelund::viewport {
namespace {
struct PlaneSpec {
    gp_Dir normal;
    gp_Dir xDirection;
    Quantity_Color color;
};
// Frames match document::datumSketchPlane: XY (+Z), XZ (x +X, y +Z, normal -Y), YZ (x +Y,
// y +Z, normal +X).
const std::array<PlaneSpec, 3>& specs() {
    static const std::array<PlaneSpec, 3> values{
        PlaneSpec{gp_Dir(0, 0, 1), gp_Dir(1, 0, 0),
                  Quantity_Color(0.30, 0.52, 0.95, Quantity_TOC_RGB)},
        PlaneSpec{gp_Dir(0, -1, 0), gp_Dir(1, 0, 0),
                  Quantity_Color(0.32, 0.78, 0.42, Quantity_TOC_RGB)},
        PlaneSpec{gp_Dir(1, 0, 0), gp_Dir(0, 1, 0),
                  Quantity_Color(0.92, 0.40, 0.34, Quantity_TOC_RGB)}};
    return values;
}
} // namespace

void BasePlanePresentation::show(const occ::handle<AIS_InteractiveContext>& context,
                                 bool emphasized, double halfSize) {
    hide(context);
    for (std::size_t index = 0; index < specs().size(); ++index) {
        const auto& spec = specs()[index];
        const gp_Ax3 frame(gp_Pnt(0, 0, 0), spec.normal, spec.xDirection);
        const auto face =
            BRepBuilderAPI_MakeFace(gp_Pln(frame), -halfSize, halfSize, -halfSize, halfSize).Face();
        occ::handle<AIS_Shape> plane = new AIS_Shape(face);
        plane->SetColor(spec.color);
        plane->SetTransparency(emphasized ? 0.7 : 0.92);
        plane->Attributes()->SetFaceBoundaryDraw(true);
        plane->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(spec.color, Aspect_TOL_SOLID, emphasized ? 1.5 : 1.0));
        // Translucent hover/selection so the plane never hides the model behind it.
        const auto highlight = [&](const Quantity_Color& color, float transparency) {
            occ::handle<Prs3d_Drawer> drawer = new Prs3d_Drawer();
            drawer->SetColor(color);
            drawer->SetTransparency(transparency);
            drawer->SetDisplayMode(AIS_Shaded);
            drawer->SetZLayer(Graphic3d_ZLayerId_Default);
            return drawer;
        };
        plane->SetDynamicHilightAttributes(
            highlight(Quantity_Color(1.0, 0.58, 0.08, Quantity_TOC_RGB), 0.55F));
        plane->SetHilightAttributes(
            highlight(Quantity_Color(0.05, 0.42, 1.0, Quantity_TOC_RGB), 0.45F));
        context->Display(plane, AIS_Shaded, AIS_Shape::SelectionMode(TopAbs_FACE), false);
        planes_[index] = plane;
    }
}

void BasePlanePresentation::hide(const occ::handle<AIS_InteractiveContext>& context) {
    for (auto& plane : planes_)
        if (!plane.IsNull()) {
            context->Remove(plane, false);
            plane.Nullify();
        }
}

std::optional<BasePlane>
BasePlanePresentation::plane(const occ::handle<AIS_InteractiveObject>& object) const {
    for (std::size_t index = 0; index < planes_.size(); ++index)
        if (!planes_[index].IsNull() && planes_[index] == object)
            return static_cast<BasePlane>(index + 1);
    return std::nullopt;
}

std::size_t BasePlanePresentation::displayedCount() const noexcept {
    std::size_t count = 0;
    for (const auto& plane : planes_)
        count += plane.IsNull() ? 0 : 1;
    return count;
}
} // namespace voelund::viewport
