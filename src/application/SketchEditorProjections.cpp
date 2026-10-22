// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/features/SketchProjection.hpp>

#include <algorithm>
#include <string>

// S17 (user decision 2026-10-14): body edges projected into the sketch. Each is a construction
// element that follows its edge and is fixed in the solver; it cannot be trimmed, split or
// rounded. Deleting the element removes the projection.
namespace voelund::application {
namespace {
[[noreturn]] void refuse(const std::string& reason) {
    throw SketchEditRefusal("Projizieren: " + reason, {});
}
} // namespace

bool SketchEditorSession::isProjected(document::SketchElementId element) const noexcept {
    return std::ranges::find(projections_, element, &document::SketchProjection::element) !=
           projections_.end();
}

std::span<const document::SketchProjection> SketchEditorSession::projections() const noexcept {
    return projections_;
}

void SketchEditorSession::refreshProjectedGeometry() {
    if (projections_.empty() || !editingFeature_)
        return;
    const auto evaluated = session_.evaluatedSketch(*editingFeature_);
    if (!evaluated)
        return;
    for (const auto& projection : projections_) {
        const auto source = std::ranges::find(evaluated->elements.values, projection.element,
                                              &document::SketchElement::id);
        const auto target =
            std::ranges::find(elements_.values, projection.element, &document::SketchElement::id);
        if (source != evaluated->elements.values.end() && target != elements_.values.end())
            target->geometry = source->geometry;
    }
}

SketchCurveEditResult SketchEditorSession::projectEdge(document::FeatureId bodyResult,
                                                       const TopoDS_Shape& edge) {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    auto named = session_.filletEdge(bodyResult, edge);
    const auto body = session_.resultBody(bodyResult);
    if (!named || !body)
        refuse("Diese Kante liegt nicht zwischen zwei benannten Flächen eines Körpers");
    named->id = 1;
    auto geometry = features::projectSketchEdge(plane_, edge);
    if (!geometry)
        refuse("Nur gerade Kanten und Kreise parallel zur Skizze lassen sich projizieren");
    for (const auto& projection : projections_)
        if (projection.body == *body && features::sameFilletEdge(projection.edge, *named))
            throw SketchEditRefusal("Projizieren: Diese Kante ist schon projiziert",
                                    {projection.element});
    if (!canAllocateIds(1, 0))
        refuse("Die Skizze hat keine freien IDs mehr");
    Rollback rollback(*this);
    clearPreview();
    const document::SketchElementId id{elements_.nextId++};
    elements_.values.push_back({id, std::move(*geometry), true});
    projections_.push_back({id, *body, *named});
    selectedElement_ = id;
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (!keepIfValid(rollback))
        refuse("Das Ergebnis lässt sich nicht lösen; nichts geändert");
    commitHistory();
    SketchCurveEditResult result;
    result.created = id;
    return result;
}

} // namespace voelund::application
