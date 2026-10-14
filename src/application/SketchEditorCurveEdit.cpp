#include "SketchConstraintRoles.hpp"
#include "SketchCurveEditPlan.hpp"
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

// S10: trim, extend and split as one undo step. Relations follow these rules:
// - A relation on a moved end point no longer holds and is removed.
// - The second piece takes over the element's end point with its relations.
// - A line whose length changes loses the relations that fix its length or extent (distance,
//   equal length, fixed, midpoint, perpendicular bisector); a split scales its distance.
// - A circle trimmed to an arc loses what only a circle carries (point on it, fixed).
// - Direction, tangency, radius and center relations stay with the element.
// - The pieces stay on one line or circle; a new end on a cutting edge is bound to it.
namespace voelund::application {
namespace {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using Position = document::SketchPointPosition;

using detail::bindsLength;
using detail::visitPoints;

} // namespace

bool SketchEditorSession::canEditCurve(SketchCurveEdit edit) const noexcept {
    if (!active_ || !selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    return match != elements_.values.end() && detail::supportsCurveEdit(edit, match->geometry);
}

SketchCurveEditResult SketchEditorSession::editCurve(SketchCurveEdit edit) {
    const std::string name = detail::curveEditName(edit);
    if (!active_)
        throw std::invalid_argument(name + ": Der Skizzeneditor ist nicht aktiv");
    if (!selectedElement_.has_value() || !selectionPoint_.has_value())
        throw std::invalid_argument(name + ": Zuerst das Element an der gewünschten Stelle "
                                           "anklicken");
    const SketchElementId target = *selectedElement_;
    if (isProjected(target)) // S17
        throw SketchEditRefusal(name + ": Eine projizierte Kante folgt ihrem Körper", {target});
    if (isPatternInstance(target))
        throw SketchEditRefusal(name + ": Eine Kopie folgt ihrem Original; das Original ändern "
                                       "oder das Muster auflösen",
                                {target});
    const auto plan = detail::planCurveEdit(edit, elements_.values, target, *selectionPoint_);
    const bool newPiece = plan.piece.has_value();
    const bool pieceOfLine = std::holds_alternative<document::SketchLine>(plan.edited);
    const std::uint64_t relations =
        plan.bindings.size() + (newPiece ? (plan.joined || !pieceOfLine ? 2 : 1) : 0);
    if (!canAllocateIds(newPiece ? 1 : 0, relations))
        throw std::invalid_argument(name + ": Die Skizze hat keine freien IDs mehr");

    Rollback rollback(*this);
    clearPreview();
    auto element = std::ranges::find(elements_.values, target, &document::SketchElement::id);
    const bool line = std::holds_alternative<document::SketchLine>(element->geometry);
    const bool circleToArc = std::holds_alternative<document::SketchCircle>(element->geometry);
    const bool construction = element->construction;
    element->geometry = plan.edited;
    SketchCurveEditResult result;
    std::optional<SketchElementId> piece;
    if (newPiece) {
        piece = SketchElementId{elements_.nextId++};
        elements_.values.push_back({*piece, *plan.piece, construction});
        result.created = piece;
    }
    const bool lengthChanged = line && (plan.movedEnd.has_value() || newPiece);
    std::vector<SketchConstraint> kept;
    for (SketchConstraint& constraint : constraints_.values) {
        bool moved = false;
        visitPoints(constraint, [&](SketchGeometryReference& reference) {
            if (reference.element != target)
                return;
            if (piece && reference.point == Position::end)
                reference.element = *piece;
            else if (plan.movedEnd && reference.point == *plan.movedEnd)
                moved = true;
        });
        const bool scaled = plan.keptShare && constraint.kind == SketchConstraintKind::distance &&
                            constraint.first.element == target;
        if (scaled)
            constraint.value *= *plan.keptShare;
        const bool drop =
            moved || (lengthChanged && !scaled && bindsLength(constraint, target)) ||
            (circleToArc &&
             ((constraint.kind == SketchConstraintKind::pointOnObject &&
               constraint.second->element == target) ||
              (constraint.kind == SketchConstraintKind::fixed &&
               constraint.first.element == target)));
        (drop ? result.removed : kept).push_back(std::move(constraint));
    }
    constraints_.values = std::move(kept);

    const auto relate = [&](SketchConstraintKind kind, SketchGeometryReference first,
                            SketchGeometryReference second) {
        result.added.push_back(document::SketchConstraintId{constraints_.nextId++});
        constraints_.values.push_back({result.added.back(), kind, first, second});
    };
    if (piece) {
        // The pieces stay on one line or circle: joined at the split point, or apart.
        if (plan.joined)
            relate(SketchConstraintKind::coincident, {target, Position::end},
                   {*piece, Position::start});
        if (line)
            relate(plan.joined ? SketchConstraintKind::parallel : SketchConstraintKind::collinear,
                   {*piece, Position::start}, {target, Position::start});
        else {
            relate(SketchConstraintKind::concentric, {*piece, Position::center},
                   {target, Position::center});
            if (!plan.joined)
                relate(SketchConstraintKind::equal, {*piece, Position::start},
                       {target, Position::start});
        }
    }
    for (const auto& binding : plan.bindings) {
        if (isPatternInstance(binding.cutter)) // S12: copies carry no relations
            continue;
        const SketchGeometryReference end{binding.piece == 0 ? target : *piece, binding.point};
        if (binding.cutterEnd.has_value())
            relate(SketchConstraintKind::coincident, end, {binding.cutter, *binding.cutterEnd});
        else
            relate(SketchConstraintKind::pointOnObject, end,
                   {binding.cutter, Position::start});
    }

    // A trimmed piece is gone from under the pick; a split selects the new piece.
    selectedReferences_.clear();
    if (edit == SketchCurveEdit::trim) {
        selectedElement_.reset();
        selectionPoint_.reset();
    } else if (piece)
        selectedElement_ = piece;
    if (!keepIfValid(rollback))
        throw std::invalid_argument(name + ": Das Ergebnis lässt sich nicht lösen; nichts "
                                           "geändert");
    return result;
}

bool SketchEditorSession::trimSelected() {
    try {
        static_cast<void>(editCurve(SketchCurveEdit::trim));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool SketchEditorSession::extendSelected() {
    try {
        static_cast<void>(editCurve(SketchCurveEdit::extend));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool SketchEditorSession::splitSelected() {
    try {
        static_cast<void>(editCurve(SketchCurveEdit::split));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace voelund::application
