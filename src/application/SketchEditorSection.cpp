// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchProjection.hpp>
#include <voelund/kernel/KernelError.hpp>

#include <algorithm>
#include <cmath>
#include <string>

// N15a (agent decision 2026-10-29): section curves, as Fusion's "Intersect" with every visible
// body at once. Where the bodies meet the sketch plane, ordinary sketch curves are added in one
// step (no link to the body; they change when edited like any curve). Curves that meet end to
// end are joined so they move together.
namespace voelund::application {
namespace {

[[noreturn]] void refuse(const std::string& reason) {
    throw SketchEditRefusal("Schnittkurve: " + reason, {});
}

// The visible, not consumed bodies of the model as it is now.
std::vector<TopoDS_Shape> visibleBodies(const CadSession& session) {
    const auto outline = session.modelOutline();
    std::vector<TopoDS_Shape> shapes;
    for (const auto& body : session.validBodies()) {
        const auto named = std::ranges::find(outline.bodies, body.bodyId, &OutlineBody::id);
        if (named == outline.bodies.end() || (!named->hidden && !named->consumed))
            shapes.push_back(body.shape);
    }
    return shapes;
}

std::vector<document::SketchGeometry> sectionCurves(const CadSession& session,
                                                    const document::SketchPlane& plane) {
    std::vector<document::SketchGeometry> curves;
    try {
        for (const auto& body : visibleBodies(session)) {
            auto found = features::sectionSketchCurves(plane, body);
            curves.insert(curves.end(), found.begin(), found.end());
        }
    } catch (const features::GeometryEvaluationError& error) {
        refuse(error.what());
    } catch (const kernel::KernelError& error) {
        refuse(error.what());
    }
    return curves;
}

// The coincidences of ends that the section curves share.
std::vector<std::pair<document::SketchGeometryReference, document::SketchGeometryReference>>
sharedEnds(const std::vector<document::SketchElement>& added) {
    using Position = document::SketchPointPosition;
    std::vector<std::pair<document::SketchGeometryReference, document::SketchGeometryReference>>
        joints;
    for (std::size_t a = 0; a < added.size(); ++a)
        for (std::size_t b = a + 1; b < added.size(); ++b)
            for (const auto first : {Position::start, Position::end})
                for (const auto second : {Position::start, Position::end}) {
                    const auto p = document::sketchReferencePoint(added[a].geometry, first);
                    const auto q = document::sketchReferencePoint(added[b].geometry, second);
                    if (p && q && std::hypot(p->x - q->x, p->y - q->y) < 1e-7)
                        joints.push_back({{added[a].id, first}, {added[b].id, second}});
                }
    return joints;
}

} // namespace

std::size_t SketchEditorSession::addSectionCurves() {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    auto curves = sectionCurves(session_, plane_);
    if (curves.empty())
        refuse("Kein sichtbarer Körper schneidet die Skizzenebene");
    std::vector<document::SketchElement> added;
    std::uint64_t next = elements_.nextId;
    for (auto& curve : curves)
        added.push_back({document::SketchElementId{next++}, std::move(curve), false});
    const auto joints = sharedEnds(added);
    if (!canAllocateIds(added.size(), joints.size()))
        refuse("Die Skizze hat keine freien IDs mehr");
    Rollback rollback(*this);
    clearPreview();
    elements_.nextId = next;
    elements_.values.insert(elements_.values.end(), added.begin(), added.end());
    for (const auto& [first, second] : joints)
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                       document::SketchConstraintKind::coincident, first, second,
                                       0.0});
    selectedElement_ = added.front().id;
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (!keepIfValid(rollback))
        refuse("Das Ergebnis lässt sich nicht lösen; nichts geändert");
    commitHistory();
    return added.size();
}

} // namespace voelund::application
