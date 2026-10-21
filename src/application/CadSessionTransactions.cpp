#include "CadSessionEvaluationDetail.hpp"
#include "CommandInput.hpp"
#include "FieldBindingDetail.hpp"
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
// The features that are new or whose parameters changed.
std::vector<document::FeatureId> changedFeatures(const document::ModelDocument& document,
                                                 const document::ModelDocument& candidate,
                                                 const document::DocumentSnapshot& evaluated) {
    std::vector<document::FeatureId> changed;
    const auto before = document::DocumentSnapshot::capture(document);
    for (const auto& feature : candidate.features())
        if (!document.contains(feature.id) ||
            evaluated.featureParameters(feature.id) != before.featureParameters(feature.id))
            changed.push_back(feature.id);
    return changed;
}
} // namespace

CommandResult CadSession::finishCandidate(document::DocumentTransaction& transaction,
                                          std::optional<document::FeatureId> selection,
                                          CommandKind kind, ErrorDisposition disposition,
                                          bool clearSketchSelection) {
    // N01b: the shell's field bindings join the undo step of the command.
    if (const auto problem =
            detail::takePendingFields(transaction.edit(), document_, pendingFields_))
        return CommandResult::invalidInput(*problem);
    auto evaluation = featureGraph_.evaluateDetailed(
        document::DocumentSnapshot::capture(transaction.candidate()), evaluation_);
    auto states = detail::runtimeStates(evaluation->report);
    auto report = evaluation->report;
    auto evaluated = evaluation->result;
    // Unchanged errors in unrelated branches do not require another error-acceptance action.
    const auto changed = changedFeatures(document_, transaction.candidate(), evaluation->snapshot);
    const auto affected = evaluation->dependencies.affected(changed);
    // M16: a deletion or a new order changes which features there are and what they take in;
    // then every error counts, not only those behind changed parameters.
    const bool structural =
        !std::ranges::equal(transaction.candidate().features(), document_.features());
    for (std::size_t i = 0; i < report.features.size(); ++i)
        if (kind != CommandKind::newFeature && !structural && !affected[i])
            report.features[i].status = features::EvaluationStatus::valid;
    CommandResult published(evaluation->valid() ? CommandStatus::committed
                                                : CommandStatus::committedWithErrors,
                            RejectionReason::none, evaluation->report);
    CommandTransaction command(kind, std::make_unique<SessionDocumentBackend>(transaction));
    auto result = command.finish(std::move(report), disposition);
    if (result.status() == CommandStatus::rejected) {
        result.evaluation_ = std::move(published.evaluation_);
        return result;
    }
    // No callbacks or allocation between document publication and session projection.
    if (evaluation->valid()) {
        currentShape_ = std::move(evaluated.resultShape);
        currentBodies_.swap(evaluated.bodies);
        currentFeatures_.swap(evaluated.features);
        geometryStamp_ = sceneStatus().document;
    }
    evaluation_ = std::move(evaluation);
    featureStates_.swap(states);
    selectedFeature_ = selection;
    if (clearSketchSelection) {
        selectedSketchElement_.reset();
        selectedSketchProfiles_.clear();
    }
    published.feature_ = selection;
    return published;
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
