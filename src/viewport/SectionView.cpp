// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SectionView.hpp"

#include <voelund/kernel/EdgeQueries.hpp>

#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

namespace voelund::viewport {
namespace {

gp_Pnt point(const std::array<double, 3>& value) { return {value[0], value[1], value[2]}; }

gp_Dir direction(const std::array<double, 3>& value) { return {value[0], value[1], value[2]}; }

// User decision 2026-10-23: the cut face in a darker shade of the body colour, no hatching.
Quantity_Color darker(const Quantity_Color& color) {
    constexpr double factor = 0.6;
    return {color.Red() * factor, color.Green() * factor, color.Blue() * factor, Quantity_TOC_RGB};
}

// The plane equation of OCCT keeps the side its normal points to.
gp_Pln clippingPlane(const SectionPlane& plane) {
    return {point(plane.origin), direction(plane.normal)};
}

} // namespace

void SectionView::set(const occ::handle<AIS_InteractiveContext>& context,
                      const std::optional<SectionPlane>& plane, const Bnd_Box& extent,
                      const Quantity_Color& gripColor) {
    if (!plane) {
        plane_.reset();
        grip_.hide();
        applyPlane();
        return;
    }
    // Only shifted along the shown normal: the grip moves, as with a typed push/pull distance.
    const gp_Dir normal = direction(plane->normal);
    const gp_Vec shift(point(base_.origin), point(plane->origin));
    const double along = shift.Dot(gp_Vec(normal));
    const bool shiftedOnly = plane_ && grip_.isShown() &&
                             normal.IsEqual(direction(base_.normal), 1e-9) &&
                             (shift - gp_Vec(normal) * along).Magnitude() < 1e-9;
    plane_ = plane;
    applyPlane();
    if (shiftedOnly) {
        grip_.setDistance(-along);
        return;
    }
    base_ = *plane;
    showGrip(context, extent, gripColor);
}

void SectionView::clip(const occ::handle<AIS_Shape>& body, const Quantity_Color& color) {
    occ::handle<Graphic3d_ClipPlane> plane = new Graphic3d_ClipPlane();
    plane->SetCapping(true);
    plane->SetCappingColor(darker(color));
    planes_.push_back(plane);
    body->AddClipPlane(plane);
    if (plane_)
        plane->SetEquation(clippingPlane(*plane_));
    plane->SetOn(plane_.has_value());
}

void SectionView::applyPlane() {
    for (const auto& plane : planes_) {
        if (plane_)
            plane->SetEquation(clippingPlane(*plane_));
        plane->SetOn(plane_.has_value());
    }
}

void SectionView::reset() {
    grip_.reset();
    planes_.clear();
}

void SectionView::setHandler(Handler handler) {
    handler_ = std::move(handler);
    grip_.setHandler([this](double distance, bool finished) { moved(distance, finished); });
}

void SectionView::showGrip(const occ::handle<AIS_InteractiveContext>& context,
                           const Bnd_Box& extent, const Quantity_Color& gripColor) {
    const gp_Pnt origin = point(base_.origin);
    const gp_Dir normal = direction(base_.normal);
    // The arrow points to the removed side, where it is free to grab. The patch covers the model:
    // centred on the foot of the model's centre on the plane.
    gp_Pnt centre = origin;
    double halfSize = 50.0;
    if (!extent.IsVoid()) {
        const gp_Pnt middle((extent.CornerMin().XYZ() + extent.CornerMax().XYZ()) * 0.5);
        centre = middle.Translated(gp_Vec(normal) * -gp_Vec(origin, middle).Dot(gp_Vec(normal)));
        halfSize = std::max(0.6 * std::sqrt(extent.SquareExtent()), 1.0);
    }
    grip_.show(context, kernel::planePatch(gp_Ax3(centre, normal), halfSize), normal.Reversed(),
               gripColor);
}

SectionPlane SectionView::shifted(double distance) const {
    const gp_Pnt origin =
        point(base_.origin).Translated(gp_Vec(direction(base_.normal)) * -distance);
    return {{origin.X(), origin.Y(), origin.Z()}, base_.normal};
}

void SectionView::moved(double distance, bool finished) {
    if (!plane_)
        return;
    plane_ = shifted(distance);
    applyPlane();
    if (handler_)
        handler_(*plane_, finished);
}

} // namespace voelund::viewport
