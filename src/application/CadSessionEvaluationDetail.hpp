#pragma once
#include <voelund/application/CadSession.hpp>
namespace voelund::application::detail {
inline std::vector<FeatureRuntimeState> runtimeStates(const features::EvaluationReport& report) {
    std::vector<FeatureRuntimeState> states;
    for (const auto& issue : report.features)
        states.push_back({issue.feature,
            issue.status == features::EvaluationStatus::valid ? FeatureRuntimeStatus::current :
            issue.status == features::EvaluationStatus::failed ? FeatureRuntimeStatus::error :
            issue.status == features::EvaluationStatus::suppressed ? FeatureRuntimeStatus::suppressed // M14
            : issue.status == features::EvaluationStatus::rolledBack ? FeatureRuntimeStatus::rolledBack
                                                                   : FeatureRuntimeStatus::blocked,
            issue.message});
    return states;
}
}
