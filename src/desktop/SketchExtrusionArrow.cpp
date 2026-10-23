// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchExtrusionToolController.hpp"

#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <QObject>
#include <QTimer>

#include <cmath>
#include <sstream>

// W03 (as in Fusion and SolidCraft): an extrusion shows an arrow on its profile along the way it
// grows; dragging the arrow sets the distance, a typed distance moves the arrow. The distance
// field takes the keyboard when the tool opens, so a number typed right away is the distance.
namespace voelund::desktop {

void SketchExtrusionToolController::syncArrow() {
    const bool straight = active_ && profileSketch_ && !revolve_ && !sweep_ && !loft_ && !upToFace_;
    if (!straight || profileIds_.empty() || viewport_ == nullptr) {
        hideArrow();
        return;
    }
    // The arrow stands on the profile and points the way the solid grows.
    std::ostringstream key;
    key << profileSketch_->value() << '/' << reversed_;
    for (const auto& id : profileIds_)
        key << ',' << id.text();
    if (arrowKey_ == key.str()) {
        viewport_->setPushPullDistance(distance_);
        return;
    }
    hideArrow();
    std::vector<TopoDS_Shape> faces;
    const auto sketch = session_.evaluatedSketch(*profileSketch_);
    try {
        if (sketch)
            for (const auto& profile : features::evaluateRegionFaces(*sketch, profileIds_))
                faces.push_back(profile.face);
    } catch (const std::exception&) {
        faces.clear(); // the preview names the problem; no arrow then
    }
    if (faces.empty())
        return;
    const auto& plane = sketch->plane;
    const double side = reversed_ ? -1.0 : 1.0;
    viewport::PlanarFaceSelection profile{};
    profile.originX = plane.originX;
    profile.originY = plane.originY;
    profile.originZ = plane.originZ;
    profile.normalX = plane.normalX * side;
    profile.normalY = plane.normalY * side;
    profile.normalZ = plane.normalZ * side;
    profile.xDirectionX = plane.xDirectionX;
    profile.xDirectionY = plane.xDirectionY;
    profile.xDirectionZ = plane.xDirectionZ;
    profile.selectedFace = kernel::collect(faces);
    viewport_->setPushPullHandler(
        [this](double distance, bool finished) { arrowDragged(distance, finished); });
    viewport_->beginPushPull(profile);
    viewport_->setPushPullDistance(distance_);
    arrowKey_ = key.str();
}

void SketchExtrusionToolController::hideArrow() {
    if (arrowKey_.empty() || viewport_ == nullptr)
        return;
    arrowKey_.clear();
    viewport_->endPushPull();
}

void SketchExtrusionToolController::arrowDragged(double distance, bool finished) {
    if (!active_)
        return;
    // The arrow measures along the way the solid grows; through the profile it stops short.
    const double length = std::max(0.001, distance);
    distanceParameter_.reset(); // PAR02: a dragged value leaves the parameter
    panel_.setLengthBinding("distance", std::nullopt);
    panel_.setLengthValue("distance", length);
    setDistance(length);
    if (finished) {
        preview();
        return;
    }
    // S14: the body follows while dragging, a preview at most every 40 ms.
    if (arrowPreviewPending_)
        return;
    arrowPreviewPending_ = true;
    QTimer::singleShot(40, &panel_, [this] {
        arrowPreviewPending_ = false;
        preview();
    });
}

// Moved here from SketchExtrusionToolController.cpp (source size).
bool SketchExtrusionToolController::toggleRegionAt(const viewport::ViewRay& ray) {
    const auto sketch = regionSketch();
    const auto crossing = sketch ? sketchPlaneCrossing(session_, *sketch, ray) : std::nullopt;
    if (!crossing || !session_.selectSketchProfileAt(*sketch, crossing->point, true))
        return false;
    regionsToggled();
    return true;
}

} // namespace voelund::desktop
