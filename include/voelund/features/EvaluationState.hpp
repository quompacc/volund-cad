#pragma once
#include <string>
#include <vector>
#include <voelund/document/FeatureId.hpp>

namespace voelund::features {
// Per-node evaluation report; published together with the matching document revision.
enum class EvaluationStatus { valid, dirty, failed, blockedByDependency, suppressed };
struct EvaluationIssue final {
    document::FeatureId feature;
    EvaluationStatus status;
    std::string code;
    std::string message;
    std::string technicalDetails;
};
struct EvaluationReport final {
    std::vector<EvaluationIssue> features;
};
} // namespace voelund::features
