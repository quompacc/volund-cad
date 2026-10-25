// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchRegionPresentation.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <limits>

namespace voelund::viewport {
namespace {

Quantity_Color occt(const QColor& color) {
    return Quantity_Color(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_sRGB);
}

// Where the view ray through the pixel meets the region's plane, and how far along the ray.
std::optional<std::pair<gp_Pnt, double>>
hitOnFace(const TopoDS_Shape& shape, const occ::handle<V3d_View>& view, int x, int y) {
    double px = 0, py = 0, pz = 0, dx = 0, dy = 0, dz = 0;
    view->ConvertWithProj(x, y, px, py, pz, dx, dy, dz);
    const gp_Pnt origin(px, py, pz);
    const gp_Vec direction(dx, dy, dz);
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        const BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane)
            continue;
        const gp_Pln plane = surface.Plane();
        const gp_Vec normal(plane.Axis().Direction());
        const double across = normal.Dot(direction);
        if (std::abs(across) < 1e-12)
            continue;
        const double along = normal.Dot(gp_Vec(origin, plane.Location())) / across;
        const gp_Pnt point = origin.Translated(direction * along);
        BRepClass_FaceClassifier inside(face, point, 1e-6);
        if (inside.State() == TopAbs_IN)
            return std::pair{point, along};
    }
    return std::nullopt;
}

} // namespace

void SketchRegionPresentation::show(const occ::handle<AIS_InteractiveContext>& context,
                                    std::span<const SceneSketchRegion> regions,
                                    const ViewportTheme& theme) {
    hide(context);
    theme_ = theme;
    for (const SceneSketchRegion& region : regions) {
        Shown shown{region, new AIS_Shape(region.face)};
        paint(shown, false);
        context->Display(shown.shape, AIS_Shaded, -1, false);
        shown_.push_back(std::move(shown));
    }
}

void SketchRegionPresentation::hide(const occ::handle<AIS_InteractiveContext>& context) {
    for (const Shown& shown : shown_)
        context->Remove(shown.shape, false);
    shown_.clear();
    hovered_.reset();
}

void SketchRegionPresentation::paint(Shown& shown, bool hovered) const {
    const bool selected = shown.region.selected;
    shown.shape->SetColor(occt(selected  ? theme_.selection
                               : hovered ? theme_.hover
                                         : theme_.sketchFree));
    shown.shape->SetTransparency(selected ? 0.5 : hovered ? 0.55 : 0.82);
    shown.shape->Attributes()->SetFaceBoundaryDraw(false);
}

bool SketchRegionPresentation::hover(const occ::handle<AIS_InteractiveContext>& context,
                                     const occ::handle<V3d_View>& view, int x, int y) {
    std::optional<std::size_t> nearest;
    double distance = std::numeric_limits<double>::max();
    for (std::size_t index = 0; index < shown_.size(); ++index)
        if (const auto hit = hitOnFace(shown_[index].region.face, view, x, y);
            hit && hit->second < distance) {
            distance = hit->second;
            nearest = index;
        }
    if (nearest == hovered_)
        return false;
    for (const auto index : {hovered_, nearest})
        if (index) {
            paint(shown_[*index], index == nearest);
            context->Redisplay(shown_[*index].shape, false);
        }
    hovered_ = nearest;
    return true;
}

std::optional<std::string> SketchRegionPresentation::hovered() const {
    return hovered_ ? std::optional{shown_[*hovered_].region.key} : std::nullopt;
}

} // namespace voelund::viewport
