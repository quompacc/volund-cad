#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

// U08: the creation step of the interactive editor, moved here from the desktop so that
// every caller gets the same elements and relations, and roles instead of ID arithmetic.
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
} // namespace

void SketchEditorSession::snapTo(SketchGeometryReference created,
                                 const std::optional<SketchSnap>& snap, SketchPoint point) {
    if (!snap.has_value())
        return;
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

std::optional<SketchCreationResult>
SketchEditorSession::createCentered(const SketchCreationInput& input) {
    // S04a: a rectangle around the picked center. A construction diagonal and a construction
    // half-diagonal tie the center to the diagonal's midpoint, so the rectangle stays centered
    // on the snapped point (e.g. the origin) when its sizes change.
    const SketchPoint center = input.first;
    const SketchPoint corner = input.second;
    const SketchPoint opposite{2 * center.x - corner.x, 2 * center.y - corner.y};
    if (!setPreview(opposite, corner))
        return std::nullopt;
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
    const SketchGeometryReference first{result->primary(), SketchPointPosition::start};
    const SketchGeometryReference second{
        *result->find(SketchCreationRole::rectangleFromSecondCorner), SketchPointPosition::start};
    const auto diagonal = addLine(opposite, corner);
    static_cast<void>(addCoincidentReferences({diagonal, SketchPointPosition::start}, first));
    static_cast<void>(addCoincidentReferences({diagonal, SketchPointPosition::end}, second));
    const auto half = addLine(center, corner);
    static_cast<void>(addCoincidentReferences({half, SketchPointPosition::end}, second));
    static_cast<void>(addMidpointReference({half, SketchPointPosition::start}, diagonal));
    snapTo({half, SketchPointPosition::start}, input.firstSnap, center);
    snapTo(second, input.secondSnap, corner);
    setTool(SketchDrawingTool::rectangle);
    if (!selectElement(result->primary()))
        static_cast<void>(selectElement(half));
    result->elements.push_back({diagonal, SketchCreationRole::centerDiagonal});
    result->elements.push_back({half, SketchCreationRole::centerHalfDiagonal});
    return result;
}

std::optional<SketchCreationResult> SketchEditorSession::create(const SketchCreationInput& in) {
    if (in.centered) {
        if (tool_ != SketchDrawingTool::rectangle)
            return std::nullopt;
        return createCentered(in);
    }
    if (!setPreview(in.first, in.second, in.third))
        return std::nullopt;
    const auto result = confirmCreation();
    if (!result)
        return std::nullopt;
    const auto id = result->primary();
    const auto radial = [&](SketchConstraintKind kind) {
        return in.relationTarget.has_value() &&
               addAutomaticRadialRelation(id, *in.relationTarget, kind);
    };
    switch (tool_) {
    case SketchDrawingTool::line:
    case SketchDrawingTool::polyline:
        snapTo({id, SketchPointPosition::start}, in.firstSnap, in.first);
        snapTo({id, SketchPointPosition::end}, in.secondSnap, in.second);
        if (in.relation == SketchAutoRelation::horizontal ||
            in.relation == SketchAutoRelation::vertical)
            static_cast<void>(setSelectedLineConstraint(
                in.relation == SketchAutoRelation::horizontal
                    ? document::SketchLineConstraint::horizontal
                    : document::SketchLineConstraint::vertical));
        else if (in.relation == SketchAutoRelation::parallel ||
                 in.relation == SketchAutoRelation::perpendicular || !in.relation.has_value())
            static_cast<void>(addAutomaticLineRelation(id));
        break;
    case SketchDrawingTool::circle:
    case SketchDrawingTool::ellipse: {
        const bool circle = tool_ == SketchDrawingTool::circle;
        const bool centerRelated =
            circle && in.relation == SketchAutoRelation::concentric &&
            radial(SketchConstraintKind::concentric);
        if (!centerRelated)
            snapTo({id, SketchPointPosition::center}, in.firstSnap, in.first);
        if (circle && in.relation == SketchAutoRelation::tangent) {
            if (in.relationTarget.has_value())
                static_cast<void>(radial(SketchConstraintKind::tangent));
            else if (in.secondSnap.has_value())
                static_cast<void>(addTangencyReference(id, in.secondSnap->element));
        } else if (circle && in.relation == SketchAutoRelation::equal) {
            static_cast<void>(radial(SketchConstraintKind::equal));
        }
        break;
    }
    case SketchDrawingTool::arc: {
        const bool centerRelated = in.relation == SketchAutoRelation::concentric &&
                                   radial(SketchConstraintKind::concentric);
        if (!centerRelated)
            snapTo({id, SketchPointPosition::center}, in.firstSnap, in.first);
        snapTo({id, SketchPointPosition::start}, in.secondSnap, in.second);
        snapTo({id, SketchPointPosition::end}, in.thirdSnap, in.third.value_or(in.second));
        if (in.relation == SketchAutoRelation::tangent) {
            if (in.relationTarget.has_value())
                static_cast<void>(radial(SketchConstraintKind::tangent));
            else if (in.thirdSnap.has_value())
                static_cast<void>(addTangencyReference(id, in.thirdSnap->element));
        } else if (in.relation == SketchAutoRelation::equal) {
            static_cast<void>(radial(SketchConstraintKind::equal));
        }
        break;
    }
    case SketchDrawingTool::threePointArc: {
        // The arc runs counterclockwise; which input point became its start depends on the
        // turn direction, so the snaps follow the stored arc.
        const auto match = std::ranges::find(elements_.values, id, &document::SketchElement::id);
        const auto& arc = std::get<document::SketchArc>(match->geometry);
        const SketchPoint arcStart{arc.center.x + arc.radius * std::cos(arc.startAngle),
                                   arc.center.y + arc.radius * std::sin(arc.startAngle)};
        const SketchPoint last = in.third.value_or(in.second);
        const bool inputStartsArc =
            std::hypot(arcStart.x - in.first.x, arcStart.y - in.first.y) < 1.0e-6;
        snapTo({id, SketchPointPosition::start}, inputStartsArc ? in.firstSnap : in.thirdSnap,
               inputStartsArc ? in.first : last);
        snapTo({id, SketchPointPosition::end}, inputStartsArc ? in.thirdSnap : in.firstSnap,
               inputStartsArc ? last : in.first);
        break;
    }
    case SketchDrawingTool::slot:
        if (const auto cap = result->find(SketchCreationRole::slotFirstCap))
            snapTo({*cap, SketchPointPosition::center}, in.firstSnap, in.first);
        if (const auto cap = result->find(SketchCreationRole::slotSecondCap))
            snapTo({*cap, SketchPointPosition::center}, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::polygon:
        snapTo({id, SketchPointPosition::start}, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::rectangle:
        snapTo({id, SketchPointPosition::start}, in.firstSnap, in.first);
        if (const auto opposite = result->find(SketchCreationRole::rectangleFromSecondCorner))
            snapTo({*opposite, SketchPointPosition::start}, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::select:
        break;
    }
    return result;
}

} // namespace voelund::application
