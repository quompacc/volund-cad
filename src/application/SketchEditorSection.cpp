// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchDxfImport.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchProjection.hpp>
#include <voelund/kernel/KernelError.hpp>

#include <algorithm>
#include <cmath>
#include <string>

// Curves from outside the sketch, added in one step as ordinary sketch curves; curves that meet
// end to end are joined so they move together (agent decisions 2026-10-29).
// - N15a: section curves, as Fusion's "Intersect" with every visible body at once, where the
//   bodies meet the sketch plane (no link to the body).
// - N15b: the drawing of a DXF file (SketchDxfImport.hpp); what it cannot take is counted.
// - N15c: the outline of a text, made by the desktop from the font (SketchText.hpp).
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

// Whether the curve on its own is valid sketch geometry (a DXF file can hold degenerate ones).
bool validCurve(const document::SketchGeometry& geometry, bool construction) {
    try {
        document::validateSketchElements(
            {{{document::SketchElementId{1}, geometry, construction}}, 2});
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace

std::size_t SketchEditorSession::addSectionCurves() {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    std::vector<std::pair<document::SketchGeometry, bool>> curves;
    for (auto& curve : sectionCurves(session_, plane_))
        curves.emplace_back(std::move(curve), false);
    if (curves.empty())
        refuse("Kein sichtbarer Körper schneidet die Skizzenebene");
    const std::size_t count = curves.size();
    addCurvesInOneStep(std::move(curves), "Schnittkurve: ");
    return count;
}

SketchDxfImportResult SketchEditorSession::importDxf(std::string_view text) {
    const auto refuseImport = [](const std::string& reason) {
        throw SketchEditRefusal("DXF importieren: " + reason, {});
    };
    if (!active_)
        refuseImport("Der Skizzeneditor ist nicht aktiv");
    detail::DxfDrawing drawing;
    try {
        drawing = detail::readDxfDrawing(text);
    } catch (const std::invalid_argument& error) {
        refuseImport(error.what());
    }
    SketchDxfImportResult result{0, std::move(drawing.skipped)};
    std::vector<std::pair<document::SketchGeometry, bool>> curves;
    for (auto& curve : drawing.curves) {
        if (validCurve(curve.geometry, curve.construction))
            curves.emplace_back(std::move(curve.geometry), curve.construction);
        else
            ++result.skipped["ungültig"];
    }
    if (curves.empty())
        refuseImport("Die Datei enthält keine übernehmbaren Kurven");
    result.added = curves.size();
    addCurvesInOneStep(std::move(curves), "DXF importieren: ");
    return result;
}

std::size_t SketchEditorSession::addTextOutline(std::vector<document::SketchGeometry> curves) {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    if (curves.empty())
        throw SketchEditRefusal("Text: Der Text hat keine Umrisse (leer oder Schrift ohne Kurven)",
                                {});
    std::vector<std::pair<document::SketchGeometry, bool>> outline;
    for (auto& curve : curves)
        outline.emplace_back(std::move(curve), false);
    addCurvesInOneStep(std::move(outline), "Text: ");
    return curves.size();
}

// Adds the curves as one undo step, joined where they share ends; refuses with `name`.
void SketchEditorSession::addCurvesInOneStep(
    std::vector<std::pair<document::SketchGeometry, bool>> curves, const std::string& name) {
    std::vector<document::SketchElement> added;
    std::uint64_t next = elements_.nextId;
    for (auto& [geometry, construction] : curves)
        added.push_back({document::SketchElementId{next++}, std::move(geometry), construction});
    const auto joints = sharedEnds(added);
    if (!canAllocateIds(added.size(), joints.size()))
        throw SketchEditRefusal(name + "Die Skizze hat keine freien IDs mehr", {});
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
        throw SketchEditRefusal(name + "Das Ergebnis lässt sich nicht lösen; nichts geändert", {});
    commitHistory();
}

} // namespace voelund::application
