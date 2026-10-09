#include "SketchConstraintRoles.hpp"
#include "SketchFilletPlan.hpp"
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <string>
#include <tuple>

// S11: a fillet between two lines as one undo step. Relations follow these rules:
// - Where the lines met in a corner, two construction lines keep that corner as a virtual
//   sharp: each runs from the arc along its line to the old corner point, and they meet there.
// - Relations of the old corner point move to the virtual corner; a line length becomes the
//   distance from the kept end to the virtual corner; a fixed line becomes the coordinates of
//   its kept end and of the virtual corner. Nothing changes its meaning.
// - What cannot keep its meaning is removed and named: relations of an end that was no corner
//   (crossing lines), equal lengths, midpoints and bisectors of a shortened line.
// - The arc is tangent to both lines at the joints and keeps the entered radius.
namespace voelund::application {
namespace {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using Position = document::SketchPointPosition;

[[noreturn]] void refuse(const std::string& reason, std::vector<SketchElementId> elements = {}) {
    throw SketchEditRefusal("Verrunden: " + reason, std::move(elements));
}

Position other(Position position) {
    return position == Position::start ? Position::end : Position::start;
}

// One shortened line and, where it met the corner, its construction line to the corner.
struct Leg final {
    const detail::SketchFilletLine& plan;
    std::optional<SketchElementId> virtualLine;
    [[nodiscard]] bool keepsCorner() const { return plan.movedFromCorner && virtualLine; }
    [[nodiscard]] SketchGeometryReference corner() const { return {*virtualLine, Position::end}; }
};

} // namespace

SketchCurveEditResult SketchEditorSession::filletCorner(double radius) {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    std::vector<SketchGeometryReference> picks;
    for (const auto& reference : selectedReferences_)
        if (std::ranges::find(picks, reference.element, &SketchGeometryReference::element) ==
            picks.end())
            picks.push_back(reference);
    if (picks.size() != 2)
        refuse("Zwei Linien wählen (Strg-Klick)");
    std::vector<SketchElementId> notLines;
    std::vector<document::SketchLine> lines;
    bool construction = true;
    for (const auto& pick : picks) {
        const auto match = std::ranges::find(elements_.values, pick.element,
                                             &document::SketchElement::id);
        const auto* line = match == elements_.values.end()
                               ? nullptr
                               : std::get_if<document::SketchLine>(&match->geometry);
        if (line == nullptr)
            notLines.push_back(pick.element);
        else {
            lines.push_back(*line);
            construction = construction && match->construction;
        }
    }
    if (!notLines.empty())
        refuse("Nur zwischen zwei Linien (UnsupportedCurve)", notLines);
    for (const auto& pick : picks)
        if (isPatternInstance(pick.element))
            refuse("Eine Kopie folgt ihrem Original; das Original verrunden oder das Muster "
                   "auflösen",
                   {pick.element});
    const auto plan = detail::planFillet(picks[0].element, lines[0], picks[0].point,
                                         picks[1].element, lines[1], picks[1].point, radius);
    const bool corner = plan.first.movedFromCorner || plan.second.movedFromCorner;
    if (!canAllocateIds(corner ? 3 : 1, corner ? 18 : 13))
        refuse("Die Skizze hat keine freien IDs mehr");

    Rollback rollback(*this);
    clearPreview();
    for (const auto* leg : {&plan.first, &plan.second})
        std::ranges::find(elements_.values, leg->id, &document::SketchElement::id)->geometry =
            leg->line;
    const SketchElementId arc{elements_.nextId++};
    elements_.values.push_back({arc, plan.arc, construction});
    Leg first{plan.first, std::nullopt}, second{plan.second, std::nullopt};
    if (corner)
        for (Leg* leg : {&first, &second}) {
            leg->virtualLine = SketchElementId{elements_.nextId++};
            const auto& line = leg->plan.line;
            const auto touch = leg->plan.moved == Position::start ? line.start : line.end;
            elements_.values.push_back(
                {*leg->virtualLine, document::SketchLine{touch, plan.corner}, true});
        }

    SketchCurveEditResult result;
    result.created = arc;
    std::vector<SketchConstraint> kept, converted;
    const auto isCorner = [&](const SketchGeometryReference& reference) {
        return corner && (reference == first.corner() || reference == second.corner());
    };
    for (SketchConstraint constraint : constraints_.values) {
        bool drop = false;
        bool replaced = false;
        detail::visitPoints(constraint, [&](SketchGeometryReference& reference) {
            for (const Leg* leg : {&first, &second})
                if (reference.element == leg->plan.id && reference.point == leg->plan.moved) {
                    if (leg->keepsCorner())
                        reference = leg->corner();
                    else
                        drop = true;
                }
        });
        for (const Leg* leg : {&first, &second}) {
            const SketchElementId id = leg->plan.id;
            const Position keptEnd = other(leg->plan.moved);
            if (constraint.kind == SketchConstraintKind::distance && constraint.first.element == id) {
                if (!leg->keepsCorner()) {
                    drop = true;
                    continue;
                }
                // The length was the distance from the kept end to the corner.
                constraint.kind = SketchConstraintKind::pointDistance;
                constraint.first = {id, keptEnd};
                constraint.second = leg->corner();
            } else if (constraint.kind == SketchConstraintKind::fixed &&
                       constraint.first.element == id) {
                if (!leg->keepsCorner()) {
                    drop = true;
                    continue;
                }
                // Fixed ends: start (value, aux[0]), end (aux[1], aux[2]).
                const auto& aux = constraint.auxiliaryValues;
                const double keptX = keptEnd == Position::start ? constraint.value : aux[1];
                const double keptY = keptEnd == Position::start ? aux[0] : aux[2];
                const double cornerX = keptEnd == Position::start ? aux[1] : constraint.value;
                const double cornerY = keptEnd == Position::start ? aux[2] : aux[0];
                for (const auto& [reference, x, y] :
                     {std::tuple{SketchGeometryReference{id, keptEnd}, keptX, keptY},
                      std::tuple{leg->corner(), cornerX, cornerY}}) {
                    converted.push_back({document::SketchConstraintId{0},
                                         SketchConstraintKind::coordinateX, reference, std::nullopt,
                                         x});
                    converted.push_back({document::SketchConstraintId{0},
                                         SketchConstraintKind::coordinateY, reference, std::nullopt,
                                         y});
                }
                replaced = true;
            } else if (detail::bindsLength(constraint, id)) {
                drop = true;
            }
        }
        // The two lines' old corner coincidence is now the virtual corner's own.
        if (!drop && constraint.kind == SketchConstraintKind::coincident &&
            isCorner(constraint.first) && isCorner(*constraint.second))
            replaced = true;
        if (drop)
            result.removed.push_back(constraint);
        else if (!replaced)
            kept.push_back(std::move(constraint));
    }
    constraints_.values = std::move(kept);
    for (auto& constraint : converted) {
        constraint.id = document::SketchConstraintId{constraints_.nextId++};
        result.added.push_back(constraint.id);
        constraints_.values.push_back(std::move(constraint));
    }

    const auto relate = [&](SketchConstraintKind kind, SketchGeometryReference a,
                            SketchGeometryReference b, double value = 0.0) {
        result.added.push_back(document::SketchConstraintId{constraints_.nextId++});
        constraints_.values.push_back({result.added.back(), kind, a, b, value});
    };
    const Position firstOnArc = plan.arcStartOnFirst ? Position::start : Position::end;
    relate(SketchConstraintKind::coincident, {plan.first.id, plan.first.moved}, {arc, firstOnArc});
    relate(SketchConstraintKind::coincident, {plan.second.id, plan.second.moved},
           {arc, other(firstOnArc)});
    relate(SketchConstraintKind::tangent, {plan.first.id, Position::start}, {arc, Position::start});
    relate(SketchConstraintKind::tangent, {plan.second.id, Position::start},
           {arc, Position::start});
    result.added.push_back(document::SketchConstraintId{constraints_.nextId++});
    constraints_.values.push_back({result.added.back(), SketchConstraintKind::radius,
                                   {arc, Position::center}, std::nullopt, radius});
    if (corner) {
        for (const Leg* leg : {&first, &second}) {
            relate(SketchConstraintKind::coincident, {*leg->virtualLine, Position::start},
                   {leg->plan.id, leg->plan.moved});
            relate(SketchConstraintKind::parallel, {*leg->virtualLine, Position::start},
                   {leg->plan.id, Position::start});
        }
        relate(SketchConstraintKind::coincident, first.corner(), second.corner());
    }

    selectedElement_ = arc;
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (!keepIfValid(rollback))
        refuse("Das Ergebnis lässt sich nicht lösen; nichts geändert",
               {plan.first.id, plan.second.id});
    return result;
}

bool SketchEditorSession::filletSelected(double radius) {
    try {
        static_cast<void>(filletCorner(radius));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace voelund::application
