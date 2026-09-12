#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace voelund::desktop {
namespace {

document::SketchPointPosition documentPoint(viewport::SketchPointKind point) {
    if (point == viewport::SketchPointKind::end)
        return document::SketchPointPosition::end;
    if (point == viewport::SketchPointKind::center)
        return document::SketchPointPosition::center;
    return document::SketchPointPosition::start;
}

void addAutomaticCoincidence(application::SketchEditorSession& editor,
                             document::SketchGeometryReference created,
                             const std::optional<viewport::SketchPointReferenceIntent>& target,
                             document::SketchPoint targetPoint) {
    if (!target.has_value())
        return;
    if (target->point == viewport::SketchPointKind::midpoint) {
        static_cast<void>(editor.addMidpointReference(
            created, document::SketchElementId{target->elementId}));
        return;
    }
    if (target->elementId == 0) {
        if (target->point == viewport::SketchPointKind::projected) {
            static_cast<void>(editor.addFixedReference(created, targetPoint));
            return;
        }
        const auto axis = target->point == viewport::SketchPointKind::origin
                              ? application::SketchAxisReference::origin
                          : target->point == viewport::SketchPointKind::xAxis
                              ? application::SketchAxisReference::xAxis
                              : application::SketchAxisReference::yAxis;
        static_cast<void>(editor.addAxisReference(created, axis));
        return;
    }
    static_cast<void>(editor.addCoincidentReferences(
        created, {document::SketchElementId{target->elementId}, documentPoint(target->point)}));
}

} // namespace

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& intent) {
    previewAutomatic_.reset();
    const auto third =
        intent.third.has_value()
            ? std::optional{document::SketchPoint{intent.third->first, intent.third->second}}
            : std::nullopt;
    if (!editor_ || !editor_->setPreview({intent.firstX, intent.firstY},
                                         {intent.secondX, intent.secondY}, third))
        return;
    const auto id = editor_->confirmPreview();
    if (id.has_value()) {
        const auto tool = editor_->tool();
        if (tool == application::SketchDrawingTool::line ||
            tool == application::SketchDrawingTool::polyline) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.firstReference, {intent.firstX, intent.firstY});
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                                    intent.secondReference, {intent.secondX, intent.secondY});
            if (intent.automaticConstraint.has_value())
                static_cast<void>(editor_->setSelectedLineConstraint(
                    *intent.automaticConstraint == viewport::SketchAutomaticConstraint::horizontal
                        ? document::SketchLineConstraint::horizontal
                        : document::SketchLineConstraint::vertical));
        } else if (tool == application::SketchDrawingTool::circle ||
                   tool == application::SketchDrawingTool::ellipse) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                                    intent.firstReference, {intent.firstX, intent.firstY});
        } else if (tool == application::SketchDrawingTool::arc) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                                    intent.firstReference, {intent.firstX, intent.firstY});
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.secondReference, {intent.secondX, intent.secondY});
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                                    intent.thirdReference, {intent.third->first, intent.third->second});
        } else if (tool == application::SketchDrawingTool::threePointArc) {
            const auto match =
                std::ranges::find(editor_->elements(), *id, &document::SketchElement::id);
            const auto& arc = std::get<document::SketchArc>(match->geometry);
            const document::SketchPoint arcStart{arc.center.x + arc.radius * std::cos(arc.startAngle),
                                                 arc.center.y + arc.radius * std::sin(arc.startAngle)};
            const bool inputStartsArc = std::hypot(arcStart.x - intent.firstX,
                                                   arcStart.y - intent.firstY) < 1.0e-6;
            addAutomaticCoincidence(
                *editor_, {*id, document::SketchPointPosition::start},
                inputStartsArc ? intent.firstReference : intent.thirdReference,
                inputStartsArc ? document::SketchPoint{intent.firstX, intent.firstY}
                               : document::SketchPoint{intent.third->first, intent.third->second});
            addAutomaticCoincidence(
                *editor_, {*id, document::SketchPointPosition::end},
                inputStartsArc ? intent.thirdReference : intent.firstReference,
                inputStartsArc ? document::SketchPoint{intent.third->first, intent.third->second}
                               : document::SketchPoint{intent.firstX, intent.firstY});
        } else if (tool == application::SketchDrawingTool::slot) {
            addAutomaticCoincidence(
                *editor_, {document::SketchElementId{id->value() + 3},
                           document::SketchPointPosition::center},
                intent.firstReference, {intent.firstX, intent.firstY});
            addAutomaticCoincidence(
                *editor_, {document::SketchElementId{id->value() + 1},
                           document::SketchPointPosition::center},
                intent.secondReference, {intent.secondX, intent.secondY});
        } else if (tool == application::SketchDrawingTool::polygon) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.secondReference, {intent.secondX, intent.secondY});
        } else {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.firstReference, {intent.firstX, intent.firstY});
            addAutomaticCoincidence(
                *editor_,
                {document::SketchElementId{id->value() + 2}, document::SketchPointPosition::start},
                intent.secondReference, {intent.secondX, intent.secondY});
        }
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (id.has_value() && editor_->tool() == application::SketchDrawingTool::polyline) {
        const auto match =
            std::ranges::find(editor_->elements(), *id, &document::SketchElement::id);
        if (match != editor_->elements().end()) {
            const auto& line = std::get<document::SketchLine>(match->geometry);
            viewport_.continueSketchPolyline(line.end.x, line.end.y, id->value());
        }
    }
    if (id.has_value())
        statusChanged_(QObject::tr("Sketch element #%1 added").arg(id->value()));
}

} // namespace voelund::desktop
