// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace {

using voelund::document::SketchArc;
using voelund::document::SketchPoint;

bool close(double actual, double expected) { return std::abs(actual - expected) < 1.0e-6; }

SketchPoint referencePoint(const voelund::application::SketchEditorSession& editor,
                           const voelund::document::SketchGeometryReference& reference) {
    const auto elements = editor.elements();
    const auto element =
        std::ranges::find(elements, reference.element, &voelund::document::SketchElement::id);
    const bool start = reference.point == voelund::document::SketchPointPosition::start;
    if (const auto* arc = std::get_if<SketchArc>(&element->geometry);
        arc && reference.point == voelund::document::SketchPointPosition::center)
        return arc->center;
    if (const auto* line = std::get_if<voelund::document::SketchLine>(&element->geometry))
        return start ? line->start : line->end;
    const auto& arc = std::get<SketchArc>(element->geometry);
    const double angle = start ? arc.startAngle : arc.endAngle;
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}

bool coincidencesHold(const voelund::application::SketchEditorSession& editor) {
    for (const auto& constraint : editor.constraints()) {
        if (constraint.kind != voelund::document::SketchConstraintKind::coincident)
            continue;
        const auto first = referencePoint(editor, constraint.first);
        const auto second = referencePoint(editor, *constraint.second);
        if (!close(first.x, second.x) || !close(first.y, second.y)) {
            std::cerr << "Slot endpoint constraint has nonzero geometric residual\n";
            return false;
        }
    }
    return true;
}

bool preservesSlotDimensions(double angle) {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const voelund::document::SketchPlane plane{0, 0, 40, 0, 0, 1, 1, 0, 0};
    voelund::application::SketchEditorSession editor(session, support, plane);
    editor.setTool(voelund::application::SketchDrawingTool::slot);
    const SketchPoint first{2, -4};
    const SketchPoint second{first.x + 12 * std::cos(angle), first.y + 12 * std::sin(angle)};
    const SketchPoint width{first.x - 3 * std::sin(angle), first.y + 3 * std::cos(angle)};
    if (!editor.setPreview(first, second, width) || !editor.confirmPreview() ||
        editor.elements().size() != 5 || !editor.canAccept() || editor.closedProfileCount() != 1)
        return false;
    const auto& right = std::get<SketchArc>(editor.elements()[1].geometry);
    const auto& left = std::get<SketchArc>(editor.elements()[3].geometry);
    // Confirming an already consistent slot must preserve its chosen dimensions.
    const bool valid = close(right.center.x, second.x) && close(right.center.y, second.y) &&
                       close(left.center.x, first.x) && close(left.center.y, first.y) &&
                       close(right.radius, 3) && close(left.radius, 3) &&
                       close(right.endAngle - right.startAngle, std::numbers::pi) &&
                       close(left.endAngle - left.startAngle, std::numbers::pi);
    if (!valid)
        std::cerr << "Slot changed during confirmation: angle=" << angle
                  << " radii=" << right.radius << ',' << left.radius << '\n';
    return valid && coincidencesHold(editor) &&
           editor.lastSolveResult().status == voelund::features::SketchSolveStatus::success;
}

} // namespace

int main() {
    for (double angle : {0.0, std::numbers::pi / 2, std::numbers::pi, 0.37}) {
        if (!preservesSlotDimensions(angle))
            return 1;
    }
    return 0;
}
