#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QObject>

#include <algorithm>

namespace voelund::desktop {
namespace {

application::SketchSnapKind snapKind(viewport::SketchPointKind point) {
    switch (point) {
    case viewport::SketchPointKind::start:
        return application::SketchSnapKind::start;
    case viewport::SketchPointKind::end:
        return application::SketchSnapKind::end;
    case viewport::SketchPointKind::center:
        return application::SketchSnapKind::center;
    case viewport::SketchPointKind::midpoint:
        return application::SketchSnapKind::midpoint;
    case viewport::SketchPointKind::origin:
        return application::SketchSnapKind::origin;
    case viewport::SketchPointKind::xAxis:
        return application::SketchSnapKind::xAxis;
    case viewport::SketchPointKind::yAxis:
        return application::SketchSnapKind::yAxis;
    case viewport::SketchPointKind::projected:
        return application::SketchSnapKind::projected;
    case viewport::SketchPointKind::onObject:
        break;
    }
    return application::SketchSnapKind::onObject;
}

std::optional<application::SketchSnap>
snap(const std::optional<viewport::SketchPointReferenceIntent>& reference) {
    if (!reference.has_value())
        return std::nullopt;
    return application::SketchSnap{snapKind(reference->point),
                                   document::SketchElementId{reference->elementId},
                                   reference->supportReferenceId,
                                   reference->supportReferenceParameter, reference->projectedEdge};
}

// Parallel to viewport::SketchAutomaticConstraint.
application::SketchAutoRelation relation(viewport::SketchAutomaticConstraint constraint) {
    return static_cast<application::SketchAutoRelation>(constraint);
}

// U08: the viewport's intent as the application's creation input; nothing else is decided
// here.
application::SketchCreationInput creationInput(const viewport::SketchElementIntent& intent,
                                               bool centered) {
    application::SketchCreationInput input{{intent.firstX, intent.firstY},
                                           {intent.secondX, intent.secondY}};
    if (intent.third)
        input.third = document::SketchPoint{intent.third->first, intent.third->second};
    input.firstSnap = snap(intent.firstReference);
    input.secondSnap = snap(intent.secondReference);
    input.thirdSnap = snap(intent.thirdReference);
    if (intent.automaticConstraint)
        input.relation = relation(*intent.automaticConstraint);
    if (intent.automaticTargetId)
        input.relationTarget = document::SketchElementId{*intent.automaticTargetId};
    input.centered = centered;
    return input;
}

} // namespace

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& intent) {
    previewAutomatic_.reset();
    if (!editor_)
        return;
    std::optional<application::SketchCreationResult> created;
    try {
        // A rejected step leaves the editor unchanged (S02).
        created = editor_->create(creationInput(intent, toolIndex_ == centerRectangleTool));
    } catch (const std::invalid_argument& error) {
        panel_.setAcceptEnabled(editor_->canAccept());
        refreshPresentation();
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (!created)
        return;
    const auto id = created->primary();
    const auto end = created->point(application::SketchCreationPoint::lineEnd);
    if (editor_->tool() == application::SketchDrawingTool::polyline && end) {
        const auto match =
            std::ranges::find(editor_->elements(), end->element, &document::SketchElement::id);
        if (match != editor_->elements().end()) {
            const auto& line = std::get<document::SketchLine>(match->geometry);
            viewport_.continueSketchPolyline(line.end.x, line.end.y, end->element.value());
        }
    }
    statusChanged_(QObject::tr("Skizzenelement #%1 hinzugefügt").arg(id.value()));
}

} // namespace voelund::desktop
