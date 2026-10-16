#include <type_traits>
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
CommandResult CadSession::undo() { return restoreHistory(false); }
CommandResult CadSession::redo() { return restoreHistory(true); }

CommandResult CadSession::restoreHistory(bool forward) {
    if (!(forward ? canRedo() : canUndo()))
        return {CommandStatus::rejected, RejectionReason::noChange};
    CadSession candidate(*this);
    const bool changed = forward ? candidate.document_.redo() : candidate.document_.undo();
    if (!changed)
        throw std::logic_error("Available history could not be restored");
    candidate.normalizeSelection();
    const bool valid = candidate.recompute();
    features::EvaluationReport report;
    for (const auto& state : candidate.featureStates_) {
        const auto status = state.status == FeatureRuntimeStatus::current
                                ? features::EvaluationStatus::valid
                            : state.status == FeatureRuntimeStatus::suppressed // M14
                                ? features::EvaluationStatus::suppressed
                            : state.status == FeatureRuntimeStatus::rolledBack
                                ? features::EvaluationStatus::rolledBack
                            : state.status == FeatureRuntimeStatus::blocked
                                ? features::EvaluationStatus::blockedByDependency
                                : features::EvaluationStatus::failed;
        report.features.push_back(
            {state.id,
             status,
             state.status == FeatureRuntimeStatus::current      ? ""
             : state.status == FeatureRuntimeStatus::suppressed ? "suppressed"
             : state.status == FeatureRuntimeStatus::rolledBack ? "rolled-back"
                                                                : "restored-feature-error",
             state.message,
             {}});
    }
    CommandResult result(valid ? CommandStatus::committed : CommandStatus::committedWithErrors,
                         RejectionReason::none, std::move(report));
    result.feature_ = candidate.selectedFeature_;
    // Prepare the diagnostic result as well as the projection before publication.
    static_assert(std::is_nothrow_move_assignable_v<CadSession>);
    static_assert(std::is_nothrow_move_constructible_v<CommandResult>);
    *this = std::move(candidate);
    return result;
}
} // namespace voelund::application
