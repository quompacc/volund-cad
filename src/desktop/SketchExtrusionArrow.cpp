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
namespace {
// The faces of the chosen regions; none when the sketch fails (the preview names the problem).
template <typename Sketch, typename Ids>
std::vector<TopoDS_Shape> regionFaces(const Sketch& sketch, const Ids& ids) {
    std::vector<TopoDS_Shape> faces;
    try {
        if (sketch)
            for (const auto& profile : features::evaluateRegionFaces(*sketch, ids))
                faces.push_back(profile.face);
    } catch (const std::exception&) {
        faces.clear();
    }
    return faces;
}

// The arrow's base: the profile on its plane, the normal the way the solid grows.
template <typename Plane>
viewport::PlanarFaceSelection arrowBase(const Plane& plane, double side,
                                        const std::vector<TopoDS_Shape>& faces) {
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
    return profile;
}
} // namespace

// W03/W05: the distance (a revolve's angle) takes the keyboard, so a number typed right away
// is the value.
void SketchExtrusionToolController::focusMainField() {
    panel_.focusLength(revolve_ ? "angle" : "distance");
}

// Only a straight extrusion of chosen regions has the arrow.
bool SketchExtrusionToolController::showsArrow() const {
    return active_ && profileSketch_ && !revolve_ && !sweep_ && !loft_ && !upToFace_ &&
           !profileIds_.empty() && viewport_ != nullptr;
}

void SketchExtrusionToolController::syncArrow() {
    if (!showsArrow()) {
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
    const auto sketch = session_.evaluatedSketch(*profileSketch_);
    const auto faces = regionFaces(sketch, profileIds_);
    if (faces.empty())
        return;
    const auto profile = arrowBase(sketch->plane, reversed_ ? -1.0 : 1.0, faces);
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
    // Let go behind the profile (as in Fusion): the extrusion turns round, and a join that now
    // goes into the material becomes a cut (and back). During the drag it stops at the profile.
    if (finished && distance < -0.001 && !symmetric_) {
        turnAround(-distance);
        return;
    }
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

void SketchExtrusionToolController::turnAround(double length) {
    hideArrow(); // shown again, turned, by the preview
    panel_.chooseOption("direction", reversed_ ? 0 : 1);
    if (operation_ == document::ExtrusionOperation::join && reversed_) {
        panel_.chooseOption("operation", static_cast<int>(document::ExtrusionOperation::cut) - 1);
        smartCut_ = true;
        autoReversed_ = false; // the user turned it; a later operation change keeps it
    } else if (operation_ == document::ExtrusionOperation::cut && smartCut_ && !reversed_) {
        panel_.chooseOption("operation", static_cast<int>(document::ExtrusionOperation::join) - 1);
        smartCut_ = false;
    }
    distanceParameter_.reset();
    panel_.setLengthBinding("distance", std::nullopt);
    panel_.setLengthValue("distance", length);
    setDistance(length);
    preview();
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
