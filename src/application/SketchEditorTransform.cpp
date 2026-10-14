#include "SketchGeometryTransform.hpp"

#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <type_traits>
#include <variant>

// S20 (user decision 2026-10-14): turning and scaling the selection about a point, as one
// step. A relation inside the selection keeps its meaning: turning removes what is bound to
// the sketch axes (horizontal/vertical, X/Y distances, coordinates), scaling multiplies its
// lengths and moves its coordinates. Fixed elements are fixed where they land. Relations to
// elements outside the selection stay; the solve decides, and a failed solve changes nothing.
namespace voelund::application {
namespace {

using document::SketchConstraint;
using document::SketchConstraintKind;
using document::SketchElementId;

bool axisBound(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::horizontal:
    case SketchConstraintKind::vertical:
    case SketchConstraintKind::horizontalPoints:
    case SketchConstraintKind::verticalPoints:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
    case SketchConstraintKind::coordinateX:
    case SketchConstraintKind::coordinateY:
        return true;
    default:
        return false;
    }
}

bool length(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::distance:
    case SketchConstraintKind::radius:
    case SketchConstraintKind::diameter:
    case SketchConstraintKind::pointDistance:
    case SketchConstraintKind::pointLineDistance:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
        return true;
    default:
        return false;
    }
}

// A fixed line or circle is fixed where it now is (start, end; center, radius).
void refix(SketchConstraint& constraint, const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry)) {
        constraint.value = line->start.x;
        constraint.auxiliaryValues = {line->start.y, line->end.x, line->end.y};
    } else if (const auto* circle = std::get_if<document::SketchCircle>(&geometry)) {
        constraint.value = circle->center.x;
        constraint.auxiliaryValues = {circle->center.y, circle->radius};
    }
}

} // namespace

std::vector<SketchElementId> SketchEditorSession::selectionIds() const {
    std::vector<SketchElementId> ids;
    if (selectedElement_)
        ids.push_back(*selectedElement_);
    for (const auto& reference : selectedReferences_)
        if (std::ranges::find(ids, reference.element) == ids.end())
            ids.push_back(reference.element);
    return ids;
}

std::optional<document::SketchPoint> SketchEditorSession::selectionCenter() const {
    double left = std::numeric_limits<double>::infinity(), bottom = left;
    double right = -left, top = -left;
    const auto add = [&](document::SketchPoint point, double reach = 0.0) {
        left = std::min(left, point.x - reach);
        right = std::max(right, point.x + reach);
        bottom = std::min(bottom, point.y - reach);
        top = std::max(top, point.y + reach);
    };
    for (const auto id : selectionIds()) {
        const auto element = std::ranges::find(elements_.values, id, &document::SketchElement::id);
        if (element == elements_.values.end())
            continue;
        std::visit(
            [&](const auto& geometry) {
                using Geometry = std::decay_t<decltype(geometry)>;
                if constexpr (std::is_same_v<Geometry, document::SketchLine>) {
                    add(geometry.start);
                    add(geometry.end);
                } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                    add(geometry.first);
                    add(geometry.second);
                } else if constexpr (std::is_same_v<Geometry, document::SketchEllipse>) {
                    add(geometry.center, geometry.majorRadius);
                } else if constexpr (std::is_same_v<Geometry, document::SketchPointGeometry>) {
                    add(geometry.position);
                } else {
                    add(geometry.center, geometry.radius); // circle, arc
                }
            },
            element->geometry);
    }
    if (left > right)
        return std::nullopt;
    return document::SketchPoint{(left + right) / 2.0, (bottom + top) / 2.0};
}

SketchCurveEditResult SketchEditorSession::rotateSelected(document::SketchPoint center,
                                                          double angle) {
    return transformSelection(false, center, angle);
}

SketchCurveEditResult SketchEditorSession::scaleSelected(document::SketchPoint center,
                                                         double factor) {
    return transformSelection(true, center, factor);
}

SketchCurveEditResult SketchEditorSession::transformSelection(bool scale,
                                                              document::SketchPoint center,
                                                              double value) {
    const std::string name = scale ? "Skalieren: " : "Drehen: ";
    const auto refuse = [&name](const std::string& reason,
                                std::vector<SketchElementId> elements = {}) {
        throw SketchEditRefusal(name + reason, std::move(elements));
    };
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    const auto ids = selectionIds();
    if (ids.empty())
        refuse("Erst Geometrie wählen");
    if (!std::isfinite(value) || !std::isfinite(center.x) || !std::isfinite(center.y))
        refuse("Der Wert ist keine Zahl");
    if (scale && value <= 0.0)
        refuse("Der Faktor muss größer als 0 sein");
    if ((scale && value == 1.0) ||
        (!scale && std::abs(std::remainder(value, 2.0 * std::numbers::pi)) < 1e-12))
        refuse(scale ? "Der Faktor 1 ändert nichts" : "Eine volle Drehung ändert nichts");
    for (const auto id : ids) {
        if (isProjected(id))
            refuse("Eine projizierte Kante folgt ihrem Körper", {id});
        if (isPatternInstance(id))
            refuse("Eine Kopie folgt ihrem Original; das Original bearbeiten oder das Muster "
                   "auflösen",
                   {id});
    }
    const auto selected = [&ids](const document::SketchGeometryReference& reference) {
        return std::ranges::find(ids, reference.element) != ids.end();
    };
    const auto transform = scale ? detail::PlanarTransform::scaling(center, value)
                                 : detail::PlanarTransform::rotation(center, value);
    for (const auto& constraint : constraints_.values) {
        const bool touches = selected(constraint.first) ||
                             (constraint.second && selected(*constraint.second));
        if (touches && (constraint.projectedEdge || (constraint.kind != SketchConstraintKind::fixed &&
                                                     !constraint.auxiliaryValues.empty())))
            refuse("Die Auswahl hängt an Kanten der Stützfläche", {constraint.first.element});
        if (touches && scale && constraint.parameter && length(constraint.kind))
            refuse("Ein an einen Parameter gebundenes Maß skaliert nicht mit; erst lösen",
                   {constraint.first.element});
    }

    Rollback rollback(*this);
    clearPreview();
    for (const auto id : ids) {
        const auto element = std::ranges::find(elements_.values, id, &document::SketchElement::id);
        if (element == elements_.values.end())
            continue;
        if (std::holds_alternative<document::SketchRectangle>(element->geometry) &&
            !transform.keepsAxes())
            refuse("Ein altes Rechteck dreht sich nur um Vielfache von 90°", {id});
        element->geometry = detail::transformedCopy(element->geometry, transform).elements.front();
    }

    SketchCurveEditResult result;
    std::vector<SketchConstraint> kept;
    for (SketchConstraint constraint : constraints_.values) {
        const bool all = selected(constraint.first) &&
                         (!constraint.second || selected(*constraint.second)) &&
                         (!constraint.third || selected(*constraint.third));
        const bool any = selected(constraint.first) ||
                         (constraint.second && selected(*constraint.second)) ||
                         (constraint.third && selected(*constraint.third));
        if (!any) {
            kept.push_back(std::move(constraint));
            continue;
        }
        if (!scale && !transform.keepsAxes() && axisBound(constraint.kind)) {
            result.removed.push_back(constraint);
            continue;
        }
        if (constraint.kind == SketchConstraintKind::fixed) {
            refix(constraint, std::ranges::find(elements_.values, constraint.first.element,
                                                &document::SketchElement::id)
                                  ->geometry);
        } else if (scale && all && length(constraint.kind)) {
            constraint.value *= value;
        } else if (scale && (constraint.kind == SketchConstraintKind::coordinateX ||
                             constraint.kind == SketchConstraintKind::coordinateY)) {
            const double origin =
                constraint.kind == SketchConstraintKind::coordinateX ? center.x : center.y;
            constraint.value = origin + (constraint.value - origin) * value;
        }
        kept.push_back(std::move(constraint));
    }
    constraints_.values = std::move(kept);
    if (!solveConstraints())
        refuse("Das Ergebnis lässt sich nicht lösen; nichts geändert", ids);
    rollback.keep();
    commitHistory();
    return result;
}

} // namespace voelund::application
