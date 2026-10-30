// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchReferencePoints.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <exception>

// S13: whole elements from a selection box, and a selected relation (user decision
// 2026-10-09: a click on a relation label selects it; Delete or "Bedingung entfernen" removes
// it). A selected relation and selected elements exclude each other.
namespace voelund::application {

bool SketchEditorSession::selectElements(std::span<const document::SketchElementId> ids,
                                         bool additive) noexcept {
    for (const auto id : ids)
        if (std::ranges::find(elements_.values, id, &document::SketchElement::id) ==
            elements_.values.end())
            return false;
    selectedConstraint_.reset();
    pickedPoints_.clear();
    std::vector<document::SketchElementId> all;
    if (additive)
        all = selectedElementIds();
    for (const auto id : ids)
        if (std::ranges::find(all, id) == all.end())
            all.push_back(id);
    if (all.empty()) {
        clearSelection();
        return true;
    }
    // One element is selected as a whole (no point reference, so it moves as a whole).
    if (all.size() == 1)
        return selectElement(all.front());
    selectedReferences_.clear();
    for (const auto id : all)
        selectedReferences_.push_back({id, document::SketchPointPosition::start});
    selectedElement_ = all.back();
    selectionPoint_.reset();
    return true;
}

bool SketchEditorSession::selectConstraint(document::SketchConstraintId id) noexcept {
    if (std::ranges::find(constraints_.values, id, &document::SketchConstraint::id) ==
        constraints_.values.end())
        return false;
    selectedElement_.reset();
    selectedReferences_.clear();
    selectionPoint_.reset();
    selectedConstraint_ = id;
    return true;
}

std::optional<document::SketchConstraintId>
SketchEditorSession::selectedConstraint() const noexcept {
    if (selectedConstraint_ &&
        std::ranges::find(constraints_.values, *selectedConstraint_,
                          &document::SketchConstraint::id) != constraints_.values.end())
        return selectedConstraint_;
    return std::nullopt;
}

std::optional<document::SketchGeometryReference>
SketchEditorSession::pointAt(document::SketchElementId elementId,
                             document::SketchPoint position) const noexcept {
    const auto match = std::ranges::find(elements_.values, elementId, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto nearest = detail::nearestReferencePoint(match->geometry, position);
    if (!nearest)
        return std::nullopt;
    const auto at = document::sketchReferencePoint(match->geometry, *nearest);
    if (!at || std::hypot(at->x - position.x, at->y - position.y) > 1.0e-6)
        return std::nullopt;
    return document::SketchGeometryReference{elementId, *nearest};
}

bool SketchEditorSession::selectPick(document::SketchElementId elementId,
                                     document::SketchPoint click,
                                     std::optional<document::SketchPoint> point, bool additive) {
    const auto reference = point ? pointAt(elementId, *point) : std::nullopt;
    if (!additive)
        pickedPoints_.clear();
    if (!selectNearestReference(elementId, reference ? *point : click, additive))
        return false;
    selectionPoint_ = click;              // trimming asks which side of the click goes
    wholePick_ = !reference && !additive; // it moves as a whole; relations still see its reference
    if (reference && std::ranges::find(pickedPoints_, *reference) == pickedPoints_.end())
        pickedPoints_.push_back(*reference);
    return true;
}

std::vector<document::SketchGeometryReference> SketchEditorSession::pickedPoints() const {
    std::vector<document::SketchGeometryReference> result;
    for (const auto& point : pickedPoints_)
        if (std::ranges::find(selectedReferences_, point) != selectedReferences_.end())
            result.push_back(point);
    return result;
}

std::optional<document::SketchPoint> SketchEditorSession::referencePosition(
    const document::SketchGeometryReference& reference) const noexcept {
    const auto match =
        std::ranges::find(elements_.values, reference.element, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    return document::sketchReferencePoint(match->geometry, reference.point);
}

bool SketchEditorSession::fixingPointDetermines(
    const document::SketchGeometryReference& reference) const {
    const auto at = referencePosition(reference);
    if (!at)
        return false;
    auto sketch = parameters(false);
    for (const auto [kind, value] : {std::pair{document::SketchConstraintKind::coordinateX, at->x},
                                     std::pair{document::SketchConstraintKind::coordinateY, at->y}})
        sketch.constraints.values.push_back(
            {document::SketchConstraintId{sketch.constraints.nextId++}, kind, reference,
             std::nullopt, value});
    try {
        if (supportBinding_)
            session_.resolveSketchSupport(*supportFeature_, sketch);
        const auto solved = features::solveSketchConstraints(sketch);
        return solved.status == features::SketchSolveStatus::success &&
               solved.degreesOfFreedom == 0;
    } catch (const std::exception&) {
        return false;
    }
}

bool SketchEditorSession::onlyPositionMissing() const {
    if (lastSolveResult_.status != features::SketchSolveStatus::success ||
        lastSolveResult_.degreesOfFreedom != 2)
        return false;
    // Any point of the sketch's own geometry tells: all of it hangs together or the answer is no.
    const auto own = std::ranges::find_if(elements_.values, [&](const auto& element) {
        return !element.construction && !detail::dragPoints(element.geometry).empty() &&
               std::ranges::find(projections_, element.id, &document::SketchProjection::element) ==
                   projections_.end();
    });
    return own != elements_.values.end() &&
           fixingPointDetermines({own->id, detail::dragPoints(own->geometry).front()});
}

} // namespace voelund::application
