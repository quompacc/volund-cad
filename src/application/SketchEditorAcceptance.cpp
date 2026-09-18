#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

namespace voelund::application {
CommandResult SketchEditorSession::tryAccept(ErrorDisposition disposition) {
    if (supportStamp_ && *supportStamp_ != session_.sceneStatus().document)
        return CommandResult::invalidInput(
            "The model changed. Reopen the sketch on its current support.");
    if (!canAccept())
        return CommandResult::invalidInput(
            "Sketch acceptance requires nonempty geometry and a successful solve");
    const auto sketch = parameters(false);
    auto result = editingFeature_.has_value()
                      ? session_.editSketchParameters(*editingFeature_, sketch, disposition)
                      : session_.createSketchFeature(supportFeature_, sketch);
    if (result.status() == CommandStatus::committed ||
        result.status() == CommandStatus::committedWithErrors ||
        (result.status() == CommandStatus::rejected &&
         result.reason() == RejectionReason::noChange)) {
        clearPreview();
        active_ = false;
    }
    return result;
}

} // namespace voelund::application
