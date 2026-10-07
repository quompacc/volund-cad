#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
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
    if (in.centered) {
        if (tool_ != SketchDrawingTool::rectangle)
            return std::nullopt;
        return createCentered(in);
    }
    if (!setPreview(in.first, in.second, in.third))
        return std::nullopt;
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
        const std::uint64_t start = constraints_.nextId;
        const bool added = add();
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
        if (in.relation == SketchAutoRelation::tangent) {
            if (in.relationTarget.has_value())
                static_cast<void>(radial(SketchConstraintKind::tangent));
            else if (in.thirdSnap.has_value())
                static_cast<void>(
                    relate([&] { return addTangencyReference(id, in.thirdSnap->element); }));
        } else if (in.relation == SketchAutoRelation::equal) {
            static_cast<void>(radial(SketchConstraintKind::equal));
        }
        break;
    }
    case SketchDrawingTool::threePointArc:
        // The middle pick only lies on the arc; it binds nothing.
        snap(SketchCreationPoint::arcStart, in.firstSnap, in.first);
        snap(SketchCreationPoint::arcEnd, in.thirdSnap, last);
        break;
    case SketchDrawingTool::slot:
        // The width pick binds nothing.
        snap(SketchCreationPoint::slotFirstCenter, in.firstSnap, in.first);
        snap(SketchCreationPoint::slotSecondCenter, in.secondSnap, in.second);
        break;
    case SketchDrawingTool::polygon:
        // No center element yet, so a snapped center pick binds nothing (S06).
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
