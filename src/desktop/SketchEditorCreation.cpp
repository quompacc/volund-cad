#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QObject>

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
                             const std::optional<viewport::SketchPointReferenceIntent>& target) {
    if (!target.has_value())
        return;
    static_cast<void>(editor.addCoincidentReferences(
        created, {document::SketchElementId{target->elementId}, documentPoint(target->point)}));
}

} // namespace

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& intent) {
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
        if (tool == application::SketchDrawingTool::line) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.firstReference);
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                                    intent.secondReference);
        } else if (tool == application::SketchDrawingTool::circle) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                                    intent.firstReference);
        } else if (tool == application::SketchDrawingTool::arc) {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                                    intent.firstReference);
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.secondReference);
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                                    intent.thirdReference);
        } else {
            addAutomaticCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                                    intent.firstReference);
            addAutomaticCoincidence(
                *editor_,
                {document::SketchElementId{id->value() + 2}, document::SketchPointPosition::start},
                intent.secondReference);
        }
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (id.has_value())
        statusChanged_(QObject::tr("Sketch element #%1 added").arg(id->value()));
}

} // namespace voelund::desktop
