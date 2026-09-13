#include <utility>
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
bool CadSession::commitCandidate(document::DocumentTransaction& transaction,
                                 std::optional<document::FeatureId> selection,
                                 bool clearSketchSelection) {
    std::vector<FeatureRuntimeState> states;
    for (const auto& feature : transaction.candidate().features())
        states.push_back({feature.id, FeatureRuntimeStatus::current, {}});
    features::FeatureGraphResult evaluated;
    bool valid = true;
    try {
        if (!transaction.candidate().features().empty())
            evaluated = featureGraph_.evaluate(transaction.candidate());
    } catch (const features::FeatureEvaluationError& error) {
        // Preserve existing editable-error semantics until the explicit D07 result/UI migration.
        valid = false;
        bool failed = false;
        for (auto& state : states) {
            if (state.id == error.featureId()) {
                failed = true;
                state.status = FeatureRuntimeStatus::error;
                state.message = error.what();
            } else if (failed) {
                state.status = FeatureRuntimeStatus::error;
                state.message = "Blocked by an earlier feature error";
            }
        }
    }
    // All evaluation and allocation precede document/history publication.
    if (!transaction.commit())
        return false;
    if (valid) {
        currentShape_ = std::move(evaluated.resultShape);
        currentBodies_.swap(evaluated.bodies);
        currentFeatures_.swap(evaluated.features);
    }
    featureStates_.swap(states);
    selectedFeature_ = selection;
    if (clearSketchSelection) {
        selectedSketchElement_.reset();
        selectedSketchProfiles_.clear();
    }
    return valid;
}
} // namespace voelund::application