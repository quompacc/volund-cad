#include "CommandInput.hpp"
#include <utility>
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
namespace {
class SessionDocumentBackend final : public CommandTransactionBackend {
  public:
    explicit SessionDocumentBackend(document::DocumentTransaction& transaction)
        : transaction_(transaction) {}
    void prepareCommit(const features::EvaluationReport&) override {
        if (!transaction_.prepare())
            throw std::logic_error("Changed command has an unchanged candidate");
    }
    void commitPrepared() noexcept override {
        // Private synchronous use: neither document nor candidate can change between
        // prepare and publication. Storage has already been reserved by prepare.
        static_cast<void>(transaction_.commit());
    }
    void rollback() noexcept override { transaction_.rollback(); }

  private:
    document::DocumentTransaction& transaction_;
};
} // namespace

CommandResult CadSession::finishCandidate(document::DocumentTransaction& transaction,
                                          std::optional<document::FeatureId> selection,
                                          CommandKind kind, ErrorDisposition disposition,
                                          bool clearSketchSelection) {
    std::vector<FeatureRuntimeState> states;
    features::EvaluationReport report;
    for (const auto& feature : transaction.candidate().features()) {
        states.push_back({feature.id, FeatureRuntimeStatus::current, {}});
        report.features.push_back({feature.id, features::EvaluationStatus::valid, {}, {}, {}});
    }
    features::FeatureGraphResult evaluated;
    bool valid = true;
    try {
        if (!transaction.candidate().features().empty())
            evaluated = featureGraph_.evaluate(transaction.candidate());
    } catch (const features::FeatureEvaluationError& error) {
        valid = false;
        bool failed = false;
        for (std::size_t i = 0; i < states.size(); ++i) {
            auto& state = states[i];
            auto& issue = report.features[i];
            if (state.id == error.featureId()) {
                failed = true;
                state.status = FeatureRuntimeStatus::error;
                state.message = error.what();
                issue.status = features::EvaluationStatus::failed;
                issue.code = "feature-evaluation-failed";
            } else if (failed) {
                state.status = FeatureRuntimeStatus::blocked;
                state.message = "Blocked by an earlier feature error";
                issue.status = features::EvaluationStatus::blockedByDependency;
                issue.code = "blocked-by-earlier-feature";
            }
            issue.message = state.message;
        }
    }
    CommandTransaction command(kind, std::make_unique<SessionDocumentBackend>(transaction));
    auto result = command.finish(std::move(report), disposition);
    if (result.status() == CommandStatus::rejected)
        return result;
    // No callbacks or allocation between document publication and session projection.
    if (valid) {
        currentShape_ = std::move(evaluated.resultShape);
        currentBodies_.swap(evaluated.bodies);
        currentFeatures_.swap(evaluated.features);
        geometryStamp_ = sceneStatus().document;
    }
    featureStates_.swap(states);
    selectedFeature_ = selection;
    if (clearSketchSelection) {
        selectedSketchElement_.reset();
        selectedSketchProfiles_.clear();
    }
    result.feature_ = selection;
    return result;
}

CommandResult CadSession::editSketchParameters(document::FeatureId featureId,
                                               const document::SketchParameters& parameters,
                                               ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    const auto input = detail::prepareInput(
        [&] { return transaction.edit().setSketchParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, featureId, CommandKind::editFeature, disposition, true);
}
} // namespace voelund::application
