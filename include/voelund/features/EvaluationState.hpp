#pragma once
#include <string>
#include <vector>
#include <voelund/document/FeatureId.hpp>

namespace voelund::features {
// Contract DTO; FeatureGraph adoption and revision binding follow D06/R01.
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