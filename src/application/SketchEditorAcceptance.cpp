#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>

namespace voelund::application {
void SketchEditorSession::clearSelection() noexcept {
    selectedElement_.reset();
    selectedReferences_.clear();
    selectedConstraint_.reset();
}

std::optional<document::SketchParameters>
SketchEditorSession::storedSketch(const CadSession& session, document::FeatureId sketch) {
    const auto features = session.features();
    const auto found = std::ranges::find(features, sketch, &document::FeatureDescriptor::id);
    if (found == features.end() || found->kind != document::FeatureKind::sketch)
        return std::nullopt;
    return session.sketchParameters(sketch);
}

CommandResult SketchEditorSession::tryAccept(ErrorDisposition disposition) {
    if (supportStamp_ && *supportStamp_ != session_.sceneStatus().document)
        return CommandResult::invalidInput(
            "The model changed. Reopen the sketch on its current support.");
    if (editingFeature_ && editedBaseline_) {
        if (storedSketch(session_, *editingFeature_) != editedBaseline_)
            return CommandResult::invalidInput(
                "The sketch was changed meanwhile. Reopen it to edit the current state.");
    }
    if (!canAccept())
        return CommandResult::invalidInput(
            "Sketch acceptance requires nonempty geometry and a successful solve");
    // AI02d: one request on the stamp the editor started from.
    auto sketch = parameters(false);
    Action action = editingFeature_.has_value()
                        ? Action{EditSketchAction{*editingFeature_, std::move(sketch)}}
                    : datum_ || constructionPlane_
                        ? Action{CreateDatumSketchAction{std::move(sketch)}}
                        : Action{CreateSketchAction{*supportFeature_, std::move(sketch)}};
    auto result = executeAction(session_, prepareRequest(session_, std::move(action), supportStamp_,
                                                         disposition))
                      .command;
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
