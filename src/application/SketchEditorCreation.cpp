#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

// U08: the creation step of the interactive editor, moved here from the desktop so that
// every caller gets the same elements and relations, and roles instead of ID arithmetic.
// S01: snaps bind only to the typed points of the result.
namespace voelund::application {
namespace {
using document::SketchConstraintKind;
using document::SketchElementId;
using document::SketchGeometryReference;
using document::SketchPoint;
using document::SketchPointPosition;

SketchPointPosition position(SketchSnapKind kind) {
    if (kind == SketchSnapKind::end)
        return SketchPointPosition::end;
    if (kind == SketchSnapKind::center)
        return SketchPointPosition::center;
    return SketchPointPosition::start;
}

[[noreturn]] void rejectSnap(SketchElementId element, const char* what) {
    throw std::invalid_argument("Fangpunkt an Element #" + std::to_string(element.value()) +
                                " abgelehnt: " + what);
}

// S01: a snap names a typed point of an existing element. One that does not fit the element
// is an error, not a dropped or invalid constraint.
void checkSnapTarget(std::span<const document::SketchElement> elements, const SketchSnap& snap) {
    const auto match = std::ranges::find(elements, snap.element, &document::SketchElement::id);
    if (match == elements.end())
        rejectSnap(snap.element, "unbekanntes Element");
    const auto& geometry = match->geometry;
    const bool line = std::holds_alternative<document::SketchLine>(geometry);
    if (snap.kind == SketchSnapKind::midpoint || snap.kind == SketchSnapKind::onObject) {
        if (!line)
            rejectSnap(snap.element, "nur auf Linien");
        return;
    }
    const bool center = snap.kind == SketchSnapKind::center;
    const bool fits = std::holds_alternative<document::SketchArc>(geometry) ||
                      (line && !center) ||
                      (center && (std::holds_alternative<document::SketchCircle>(geometry) ||
                                  std::holds_alternative<document::SketchEllipse>(geometry)));
    if (!fits)
        rejectSnap(snap.element, "das Element hat keinen solchen Punkt");
}

// S05: why the picks give no valid element, named for the tool. Points closer than this
// count as the same point.
std::string creationProblem(SketchDrawingTool tool, const SketchCreationInput& in) {
    constexpr double same = 1e-9;
    const auto apart = [](SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); };
    const SketchPoint last = in.third.value_or(in.second);
    switch (tool) {
    case SketchDrawingTool::line:
    case SketchDrawingTool::polyline:
        if (apart(in.first, in.second) <= same)
            return "Linie: Anfangs- und Endpunkt fallen zusammen";
        break;
    case SketchDrawingTool::rectangle:
        if (std::abs(in.second.x - in.first.x) <= same || std::abs(in.second.y - in.first.y) <= same)
            return in.centered ? "Rechteck aus dem Mittelpunkt: Breite oder Höhe ist null"
                               : "Rechteck: Breite oder Höhe ist null";
        break;
    case SketchDrawingTool::circle:
        if (apart(in.first, in.second) <= same)
            return "Kreis: Der Radius ist null";
        break;
    case SketchDrawingTool::arc:
        if (apart(in.first, in.second) <= same)
            return "Bogen: Der Anfangspunkt liegt im Mittelpunkt";
        if (apart(in.first, last) <= same)
            return "Bogen: Der Endpunkt liegt im Mittelpunkt";
        if (std::abs(std::atan2(in.second.y - in.first.y, in.second.x - in.first.x) -
                     std::atan2(last.y - in.first.y, last.x - in.first.x)) <= same)
            return "Bogen: Anfangs- und Endpunkt fallen zusammen";
        break;
    case SketchDrawingTool::threePointArc:
        if (apart(in.first, last) <= same || apart(in.first, in.second) <= same ||
            apart(in.second, last) <= same)
            return "Dreipunktbogen: Zwei Punkte fallen zusammen";
        return "Dreipunktbogen: Die drei Punkte liegen auf einer Geraden";
    case SketchDrawingTool::ellipse:
        if (apart(in.first, in.second) <= same)
            return "Ellipse: Die große Halbachse ist null";
        return "Ellipse: Die kleine Halbachse ist null";
    case SketchDrawingTool::slot:
        if (apart(in.first, in.second) <= same)
            return "Langloch: Die beiden Mittelpunkte fallen zusammen";
        return "Langloch: Die Breite ist null";
    case SketchDrawingTool::polygon:
        if (apart(in.first, in.second) <= same)
            return "Polygon: Der Eckpunkt liegt im Mittelpunkt";
        break;
    case SketchDrawingTool::select:
        break;
    }
    return "Die Punkte ergeben kein gültiges Skizzenelement";
}

SketchGeometryReference required(const SketchCreationResult& result, SketchCreationPoint role) {
    const auto point = result.point(role);
    if (!point.has_value())
        throw std::logic_error("Skizzenerzeugung: Punktrolle fehlt");
    return *point;
}
} // namespace

void SketchEditorSession::snapTo(SketchGeometryReference created,
                                 const std::optional<SketchSnap>& snap, SketchPoint point) {
    if (!snap.has_value())
        return;
    if (snap->element.value() != 0)
        checkSnapTarget(elements_.values, *snap);
    if (snap->kind == SketchSnapKind::midpoint) {
        static_cast<void>(addMidpointReference(created, snap->element));
        return;
    }
    if (snap->kind == SketchSnapKind::onObject) {
        static_cast<void>(addPointOnObjectReference(created, snap->element));
        return;
    }
    if (snap->element.value() == 0) {
        if (snap->kind == SketchSnapKind::projected) {
            if (snap->projectedEdge) {
                if (!addProjectedEdgeReference(created, *snap->projectedEdge))
                    throw std::invalid_argument("Anbindung an die projizierte Kante abgelehnt");
                return;
            }
            static_cast<void>(
                addSupportReference(created, point, snap->supportReference, snap->supportParameter));
            return;
        }
        const auto axis = snap->kind == SketchSnapKind::origin ? SketchAxisReference::origin
                          : snap->kind == SketchSnapKind::xAxis ? SketchAxisReference::xAxis
                                                                : SketchAxisReference::yAxis;
        static_cast<void>(addAxisReference(created, axis));
        return;
    }
    static_cast<void>(addCoincidentReferences(created, {snap->element, position(snap->kind)}));
}

void SketchEditorSession::record(SketchCreationResult& result, std::uint64_t firstConstraint,
                                 SketchConstraintOrigin origin) const {
    for (const auto& constraint : constraints_.values)
        if (constraint.id.value() >= firstConstraint &&
            std::ranges::find(result.constraints, constraint.id, &SketchCreatedConstraint::id) ==
                result.constraints.end())
            result.constraints.push_back({constraint.id, origin});
}

std::optional<SketchCreationResult>
SketchEditorSession::createCentered(const SketchCreationInput& input) {
    // S04a: a rectangle around the picked center. A construction diagonal and a construction
    // half-diagonal tie the center to the diagonal's midpoint, so the rectangle stays centered
    // on the snapped point (e.g. the origin) when its sizes change.
    const SketchPoint center = input.first;
    const SketchPoint corner = input.second;
    const SketchPoint opposite{2 * center.x - corner.x, 2 * center.y - corner.y};
    if (!setPreview(opposite, corner))
        throw std::invalid_argument(creationProblem(SketchDrawingTool::rectangle, input));
    auto result = confirmCreation();
    if (!result)
        return std::nullopt;
    const auto addLine = [this](SketchPoint from, SketchPoint to) {
        setTool(SketchDrawingTool::line);
        if (!setPreview(from, to))
            throw std::invalid_argument("Rechteck aus dem Mittelpunkt: ungültige Größe");
        const auto id = confirmPreview();
        if (!id.has_value() || !selectElement(*id) || !toggleSelectedConstruction())
            throw std::invalid_argument("Rechteck aus dem Mittelpunkt: Hilfslinie abgelehnt");
        return *id;
    };
    const auto first = required(*result, SketchCreationPoint::rectangleFirstCorner);
    const auto second = required(*result, SketchCreationPoint::rectangleSecondCorner);
    const std::uint64_t shapeStart = constraints_.nextId;
    const auto diagonal = addLine(opposite, corner);
    static_cast<void>(addCoincidentReferences({diagonal, SketchPointPosition::start}, first));
    static_cast<void>(addCoincidentReferences({diagonal, SketchPointPosition::end}, second));
    const auto half = addLine(center, corner);
    static_cast<void>(addCoincidentReferences({half, SketchPointPosition::end}, second));
    static_cast<void>(addMidpointReference({half, SketchPointPosition::start}, diagonal));
    record(*result, shapeStart, SketchConstraintOrigin::shape);
    const SketchGeometryReference middle{half, SketchPointPosition::start};
    const std::uint64_t snapStart = constraints_.nextId;
    snapTo(middle, input.firstSnap, center);
    snapTo(second, input.secondSnap, corner);
    record(*result, snapStart, SketchConstraintOrigin::snap);
    setTool(SketchDrawingTool::rectangle);
    if (!selectElement(result->primary()))
        static_cast<void>(selectElement(half));
    result->elements.push_back({diagonal, SketchCreationRole::centerDiagonal, 0});
    result->elements.push_back({half, SketchCreationRole::centerHalfDiagonal, 0});
    result->points.push_back({SketchCreationPoint::rectangleCenter, middle});
    return result;
}

std::optional<SketchCreationResult> SketchEditorSession::create(const SketchCreationInput& in) {
    // S02: a rejected snap leaves the editor as before the step, tool included.
    const SketchDrawingTool tool = tool_;
    Rollback rollback(*this);
    try {
        auto result = createStep(in);
        rollback.keep();
        return result;
    } catch (...) {
        tool_ = tool;
        clearPreview();
        throw;
    }
}

std::optional<SketchCreationResult> SketchEditorSession::createStep(const SketchCreationInput& in) {
    if (in.centered) {
        if (tool_ != SketchDrawingTool::rectangle)
            return std::nullopt;
        return createCentered(in);
    }
    // S05: degenerate picks are refused with the reason; the caller's editor stays as it was.
    if (!setPreview(in.first, in.second, in.third)) {
        if (!active_ || tool_ == SketchDrawingTool::select)
            return std::nullopt;
        throw std::invalid_argument(creationProblem(tool_, in));
    }
    auto result = confirmCreation();
    if (!result)
        return std::nullopt;
    const auto id = result->primary();
    // Each snap goes to a typed point of the result; every new constraint is recorded with
    // its origin as it is added.
    const auto snap = [&](SketchCreationPoint role, const std::optional<SketchSnap>& snapped,
                          SketchPoint point) {
        const std::uint64_t start = constraints_.nextId;
        snapTo(required(*result, role), snapped, point);
        record(*result, start, SketchConstraintOrigin::snap);
    };
    const auto relate = [&](auto&& add) {
        if (!in.automaticRelations)
            return false;
        const std::uint64_t start = constraints_.nextId;
        static_cast<void>(solveConstraints());
        const int freedom = lastSolveResult_.degreesOfFreedom;
        Rollback redundant(*this);
        const bool added = add();
        // S04: a suggested relation that the snaps already imply is not added on top: it
        // would be redundant and leave the sketch one relation from a conflict.
        if (added && freedom >= 0 && lastSolveResult_.degreesOfFreedom >= freedom)
            return false;
        redundant.keep();
        record(*result, start, SketchConstraintOrigin::relation);
        return added;
    };
    const auto radial = [&](SketchConstraintKind kind) {
        return in.relationTarget.has_value() && relate([&] {
                   return addAutomaticRadialRelation(id, *in.relationTarget, kind);
               });
    };
    const SketchPoint last = in.third.value_or(in.second);
    switch (tool_) {
    case SketchDrawingTool::line:
    case SketchDrawingTool::polyline:
        snap(SketchCreationPoint::lineStart, in.firstSnap, in.first);
        snap(SketchCreationPoint::lineEnd, in.secondSnap, in.second);
        if (in.relation == SketchAutoRelation::horizontal ||
            in.relation == SketchAutoRelation::vertical)
            static_cast<void>(relate([&] {
                return selectElement(id) &&
                       setSelectedLineConstraint(in.relation == SketchAutoRelation::horizontal
                                                     ? document::SketchLineConstraint::horizontal
                                                     : document::SketchLineConstraint::vertical);
            }));
        else if (in.relation == SketchAutoRelation::parallel ||
                 in.relation == SketchAutoRelation::perpendicular || !in.relation.has_value())
            static_cast<void>(relate([&] { return addAutomaticLineRelation(id).has_value(); }));
        break;
    case SketchDrawingTool::circle:
    case SketchDrawingTool::ellipse: {
        const bool circle = tool_ == SketchDrawingTool::circle;
        const bool centerRelated =
            circle && in.relation == SketchAutoRelation::concentric &&
            radial(SketchConstraintKind::concentric);
        if (!centerRelated)
            snap(SketchCreationPoint::center, in.firstSnap, in.first);
        if (circle && in.relation == SketchAutoRelation::tangent) {
            if (in.relationTarget.has_value())
                static_cast<void>(radial(SketchConstraintKind::tangent));
            else if (in.secondSnap.has_value())
                static_cast<void>(
                    relate([&] { return addTangencyReference(id, in.secondSnap->element); }));
        } else if (circle && in.relation == SketchAutoRelation::equal) {
            static_cast<void>(radial(SketchConstraintKind::equal));
        }
        break;
    }
    case SketchDrawingTool::arc: {
        const bool centerRelated = in.relation == SketchAutoRelation::concentric &&
                                   radial(SketchConstraintKind::concentric);
        if (!centerRelated)
            snap(SketchCreationPoint::center, in.firstSnap, in.first);
        snap(SketchCreationPoint::arcStart, in.secondSnap, in.second);
        snap(SketchCreationPoint::arcEnd, in.thirdSnap, last);
        // S05: tangent to a circle or arc only; an arc ending on a line keeps that end binding,
        // and tangency there is not supported by the solver adapter, so it is not claimed.
        if (in.relation == SketchAutoRelation::tangent && in.relationTarget.has_value()) {
            static_cast<void>(radial(SketchConstraintKind::tangent));
        } else if (in.relation == SketchAutoRelation::equal) {
            static_cast<void>(radial(SketchConstraintKind::equal));
        }
        break;
    }
    case SketchDrawingTool::threePointArc:
        // Picks: start, end, point on the arc; the last one binds nothing.
        snap(SketchCreationPoint::arcStart, in.firstSnap, in.first);
        snap(SketchCreationPoint::arcEnd, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::slot:
        // The width pick binds nothing.
        snap(SketchCreationPoint::slotFirstCenter, in.firstSnap, in.first);
        snap(SketchCreationPoint::slotSecondCenter, in.secondSnap, in.second);
        // S06: a requested horizontal or vertical slot holds its axis that way.
        if (in.relation == SketchAutoRelation::horizontal ||
            in.relation == SketchAutoRelation::vertical) {
            const auto axis = result->find(SketchCreationRole::slotAxis);
            static_cast<void>(relate([&] {
                return axis && selectElement(*axis) &&
                       setSelectedLineConstraint(in.relation == SketchAutoRelation::horizontal
                                                     ? document::SketchLineConstraint::horizontal
                                                     : document::SketchLineConstraint::vertical);
            }));
            static_cast<void>(selectElement(id));
        }
        break;
    case SketchDrawingTool::polygon:
        // S06: the center pick binds the construction circle's center.
        snap(SketchCreationPoint::polygonCenter, in.firstSnap, in.first);
        snap(SketchCreationPoint::polygonVertex, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::rectangle:
        snap(SketchCreationPoint::rectangleFirstCorner, in.firstSnap, in.first);
        snap(SketchCreationPoint::rectangleSecondCorner, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::select:
        break;
    }
    return result;
}

} // namespace voelund::application
