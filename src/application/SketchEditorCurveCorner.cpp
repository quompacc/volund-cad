// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchConstraintRoles.hpp"
#include "SketchCurveCornerPlan.hpp"
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <string>

// N14a (agent decision 2026-10-29): a fillet or chamfer where a line and an arc, or two arcs,
// meet at a common end, as one undo step. The curves are cut back to the joints; the fillet arc
// is tangent to both and keeps the entered radius, the chamfer line joins the two joints.
// Relations of the old corner end cannot keep their meaning (no virtual corner for curves) and
// are removed and listed, as are a line's length and an arc's fixing.
namespace voelund::application {
namespace {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using Position = document::SketchPointPosition;

Position other(Position position) {
    return position == Position::start ? Position::end : Position::start;
}

// Whether the relation loses its meaning when the leg's corner end moves.
bool lostWith(SketchConstraint constraint, const detail::CurveCornerLeg& leg) {
    bool movedEnd = false;
    detail::visitPoints(constraint, [&](const SketchGeometryReference& reference) {
        movedEnd = movedEnd || (reference.element == leg.id && reference.point == leg.moved);
    });
    if (movedEnd)
        return true;
    if (std::holds_alternative<document::SketchLine>(leg.geometry))
        return detail::bindsLength(constraint, leg.id);
    return constraint.kind == SketchConstraintKind::fixed && constraint.first.element == leg.id;
}

std::string prefix(bool chamfer) { return chamfer ? "Fasen: " : "Verrunden: "; }

// The two picked curves; anything but lines and arcs is refused.
std::vector<detail::CornerCurve> cornerCurves(const document::SketchElements& elements,
                                              const std::vector<SketchGeometryReference>& picks,
                                              bool chamfer, bool& construction) {
    std::vector<detail::CornerCurve> curves;
    for (const auto& pick : picks) {
        const auto match =
            std::ranges::find(elements.values, pick.element, &document::SketchElement::id);
        if (match == elements.values.end() ||
            (!std::holds_alternative<document::SketchLine>(match->geometry) &&
             !std::holds_alternative<document::SketchArc>(match->geometry)))
            throw SketchEditRefusal(prefix(chamfer) +
                                        "Nur zwischen Linien und Bögen (UnsupportedCurve)",
                                    {pick.element});
        curves.push_back({pick.element, match->geometry});
        construction = construction && match->construction;
    }
    return curves;
}

// Takes the relations that lose their meaning out of `constraints` and returns them.
std::vector<SketchConstraint> takeLost(std::vector<SketchConstraint>& constraints,
                                       const detail::CurveCornerPlan& plan) {
    std::vector<SketchConstraint> kept, lost;
    for (auto& constraint : constraints)
        (lostWith(constraint, plan.first) || lostWith(constraint, plan.second) ? lost : kept)
            .push_back(std::move(constraint));
    constraints = std::move(kept);
    return lost;
}

SketchConstraint relation(SketchConstraintKind kind, SketchGeometryReference first,
                          std::optional<SketchGeometryReference> second, double value = 0.0) {
    return {document::SketchConstraintId{0}, kind, first, second, value};
}

// The joints' coincidences and, for a fillet, both tangencies and the radius (ids still 0).
std::vector<SketchConstraint> jointRelations(const detail::CurveCornerPlan& plan,
                                             SketchElementId joint, bool chamfer, double size) {
    const Position firstOnJoint = chamfer || plan.arcStartOnFirst ? Position::start : Position::end;
    std::vector<SketchConstraint> relations{
        relation(SketchConstraintKind::coincident, {plan.first.id, plan.first.moved},
                 SketchGeometryReference{joint, firstOnJoint}),
        relation(SketchConstraintKind::coincident, {plan.second.id, plan.second.moved},
                 SketchGeometryReference{joint, other(firstOnJoint)})};
    if (chamfer)
        return relations;
    for (const auto* leg : {&plan.first, &plan.second})
        relations.push_back(relation(SketchConstraintKind::tangent, {leg->id, Position::start},
                                     SketchGeometryReference{joint, Position::start}));
    relations.push_back(
        relation(SketchConstraintKind::radius, {joint, Position::center}, std::nullopt, size));
    return relations;
}

} // namespace

SketchCurveEditResult
SketchEditorSession::treatCurveCorner(bool chamfer, double size,
                                      const std::vector<SketchGeometryReference>& picks) {
    bool construction = true;
    const auto curves = cornerCurves(elements_, picks, chamfer, construction);
    const auto plan = detail::planCurveCorner(chamfer ? detail::SketchCornerKind::chamfer
                                                      : detail::SketchCornerKind::fillet,
                                              curves[0], curves[1], size);
    if (!canAllocateIds(1, 5))
        throw SketchEditRefusal("Die Skizze hat keine freien IDs mehr", {});

    Rollback rollback(*this);
    clearPreview();
    for (const auto* leg : {&plan.first, &plan.second})
        std::ranges::find(elements_.values, leg->id, &document::SketchElement::id)->geometry =
            leg->geometry;
    const SketchElementId joint{elements_.nextId++};
    elements_.values.push_back(chamfer ? document::SketchElement{joint, plan.chamfer, construction}
                                       : document::SketchElement{joint, plan.arc, construction});
    SketchCurveEditResult result;
    result.created = joint;
    result.removed = takeLost(constraints_.values, plan);
    for (auto relation : jointRelations(plan, joint, chamfer, size)) {
        relation.id = document::SketchConstraintId{constraints_.nextId++};
        result.added.push_back(relation.id);
        constraints_.values.push_back(std::move(relation));
    }

    selectedElement_ = joint;
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (!keepIfValid(rollback))
        throw SketchEditRefusal(prefix(chamfer) +
                                    "Das Ergebnis lässt sich nicht lösen; nichts geändert",
                                {plan.first.id, plan.second.id});
    return result;
}

} // namespace voelund::application
