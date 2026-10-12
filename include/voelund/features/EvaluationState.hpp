#pragma once
#include <cstdint>
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
    // M04: sketch elements of a failed region's contour (GeometryEvaluationError::elements);
    // M05: for fillet-* codes the 1-based numbers of the fillet's edges at fault.
    std::vector<std::uint64_t> elements{};
};
struct EvaluationReport final {
    std::vector<EvaluationIssue> features;
};
} // namespace voelund::features
