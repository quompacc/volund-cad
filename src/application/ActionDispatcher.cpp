#include <voelund/application/Actions.hpp>

#include <algorithm>

namespace voelund::application {

ActionPreview previewAction(const CadSession& session, const ActionRequest& request) {
    // The copy shares immutable document states and the evaluation cache; its commits never
    // reach the session it was copied from.
    auto candidate = std::make_shared<CadSession>(session);
    ActionPreview preview{executeAction(*candidate, request), {}};
    preview.result.stamp = session.sceneStatus().document;
    if (preview.result.command.status() != CommandStatus::rejected) {
        const auto bodies = candidate->validBodies();
        preview.bodies.assign(bodies.begin(), bodies.end());
        preview.candidate = std::move(candidate);
    }
    return preview;
}

ActionResult ActionDispatcher::execute(const ActionRequest& request) {
    if (request.requestId.empty())
        return {CommandResult::invalidInput("The request has no ID"),
                ActionRejection::missingRequestId,
                session_.sceneStatus().document,
                {},
                {},
                {},
                {},
                false};
    // Requests of an earlier document generation can no longer be repeated or conflict.
    if (const auto generation = session_.sceneStatus().document.generation;
        generation != generation_) {
        entries_.clear();
        generation_ = generation;
    }
    // Checked before the stamp: a retry carries the stamp its first execution consumed.
    const auto known = std::ranges::find(
        entries_, request.requestId,
        [](const Entry& entry) -> const std::string& { return entry.request.requestId; });
    if (known != entries_.end()) {
        if (!(known->request == request))
            return {CommandResult::invalidInput("The request ID was used for other content"),
                    ActionRejection::requestConflict,
                    session_.sceneStatus().document,
                    {},
                    {},
                    {},
                    {},
                    false};
        auto repeated = known->result;
        repeated.repeated = true;
        return repeated;
    }
    auto result = executeAction(session_, request);
    if (entries_.size() == capacity)
        entries_.pop_front();
    entries_.push_back({request, result});
    return result;
}

} // namespace voelund::application
