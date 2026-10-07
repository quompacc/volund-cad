#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include <deque>
#include <set>

namespace voelund::application {

// U10: a blocked feature points at the failed feature that blocks it, through its inputs.
std::optional<document::FeatureId> CadSession::errorCause(document::FeatureId feature) const {
    if (!evaluation_)
        return std::nullopt;
    const auto& issues = evaluation_->report.features;
    const auto issue = [&](document::FeatureId id) {
        return std::ranges::find(issues, id, &features::EvaluationIssue::feature);
    };
    const auto start = issue(feature);
    if (start == issues.end() || start->status != features::EvaluationStatus::blockedByDependency)
        return std::nullopt;
    const auto nodes = evaluation_->dependencies.nodes();
    std::deque<document::FeatureId> open{feature};
    std::set<std::uint64_t> seen;
    while (!open.empty()) {
        const auto current = open.front();
        open.pop_front();
        if (!seen.insert(current.value()).second)
            continue;
        const auto node = std::ranges::find(nodes, current, &features::FeatureInputs::feature);
        if (node == nodes.end())
            continue;
        for (const auto input : node->inputs) {
            const auto found = issue(input);
            if (found != issues.end() && found->status == features::EvaluationStatus::failed)
                return input;
            open.push_back(input);
        }
    }
    return std::nullopt;
}

} // namespace voelund::application
