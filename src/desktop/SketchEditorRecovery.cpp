#include "SketchEditorToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <QObject>

#include <exception>

// L05b: the open sketch draft for the recovery entry, and its restoration.
namespace voelund::desktop {

std::optional<SketchEditorToolController::RecoveryDraft>
SketchEditorToolController::recoveryDraft() const {
    // An editor without a change since it opened has nothing to recover.
    if (!editor_ || !editor_->isActive() || !editor_->canUndo())
        return std::nullopt;
    return RecoveryDraft{draftEdited_, draftSupport_, editor_->draftParameters()};
}

bool SketchEditorToolController::restoreDraft(const application::CadSession::SketchDraft& draft,
                                              std::optional<document::FeatureId> edited) {
    const auto& sketch = draft.sketch;
    try {
        if (edited) {
            if (!session_.selectFeature(*edited) || !beginSelectedSketch())
                return false;
        } else if (sketch.datum) {
            beginOnBasePlane(*sketch.datum);
        } else if (draft.support && sketch.supportBinding) {
            const auto& plane = sketch.plane;
            viewport::PlanarFaceSelection drawing{
                draft.support->value(), plane.originX,     plane.originY,     plane.originZ,
                plane.normalX,          plane.normalY,     plane.normalZ,     plane.xDirectionX,
                plane.xDirectionY,      plane.xDirectionZ};
            setSupportProjection(drawing,
                                 session_.projectSketchSupport(*draft.support, *sketch.supportBinding));
            startOnSupport(drawing, *draft.support, plane, *sketch.supportBinding);
        } else {
            return false;
        }
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return false;
    }
    if (!editor_ || !editor_->loadDraft(sketch.elements, sketch.constraints, sketch.patterns)) {
        cancel();
        return false;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzenentwurf wiederhergestellt"));
    return true;
}

} // namespace voelund::desktop
