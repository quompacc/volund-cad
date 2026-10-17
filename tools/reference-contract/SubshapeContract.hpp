#pragma once

// R02 decision prototype. Not linked into the product; see docs/subshape-reference-contract.md.
#include <voelund/document/BodyId.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/FeatureId.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace voelund::reference_contract {
enum class Topology { face, edge };
enum class BoxSide { negativeX, positiveX, negativeY, positiveY, negativeZ, positiveZ };
enum class Cap { start, end };
enum class ElementPart { wholeCurve, rectangleBottom, rectangleRight, rectangleTop, rectangleLeft };

struct BoxFaceKey {
    BoxSide side;
    bool operator==(const BoxFaceKey&) const = default;
};
// Region token is an opaque semantic boundary identity, NOT the current SketchProfileId.
// R09 owns its production allocation/matching; R02 fixtures use explicit symbolic tokens.
struct ExtrusionCapKey {
    std::string region;
    Cap cap;
    bool operator==(const ExtrusionCapKey&) const = default;
};
struct ContourEdgeKey {
    document::FeatureId sketch;
    std::uint64_t element;
    ElementPart part;
    bool operator==(const ContourEdgeKey&) const = default;
};
struct ExtrusionSideKey {
    std::string region;
    ContourEdgeKey boundary;
    bool operator==(const ExtrusionSideKey&) const = default;
};
using OriginKey = std::variant<BoxFaceKey, ExtrusionCapKey, ContourEdgeKey, ExtrusionSideKey>;

struct SubshapeRef {
    document::DocumentId document;
    document::BodyId body;
    document::FeatureId originFeature;
    Topology topology;
    OriginKey origin;
    bool operator==(const SubshapeRef&) const = default;
};
struct EvaluationScope {
    document::DocumentId document;
    document::BodyId body;
    document::FeatureId outputFeature;
    std::uint64_t revision;
    bool operator==(const EvaluationScope&) const = default;
};
// Tokens identify distinct shapes in ONE evaluation only. Never serialize them as references.
struct Candidate {
    std::string token;
    Topology topology;
    bool operator==(const Candidate&) const = default;
};
enum class Coverage { complete, deleted, unavailable };
struct LineageProjection {
    SubshapeRef source;
    EvaluationScope target;
    Coverage coverage;
    // Complete proven descendants, or repair suggestions when coverage is unavailable.
    std::vector<Candidate> candidates;
};
enum class Status { resolved, missing, ambiguous };
enum class Reason { unique, split, deleted, historyUnavailable, wrongScope, unknownOrigin };
struct Resolution {
    Status status;
    Reason reason;
    EvaluationScope scope;
    std::vector<Candidate> candidates;
};

inline bool validKey(const SubshapeRef& ref) {
    if (ref.originFeature.value() == 0)
        return false;
    return std::visit(
        [&](const auto& key) {
            using T = std::decay_t<decltype(key)>;
            if constexpr (std::is_same_v<T, BoxFaceKey>)
                return ref.topology == Topology::face;
            else if constexpr (std::is_same_v<T, ExtrusionCapKey>)
                return ref.topology == Topology::face && !key.region.empty();
            else if constexpr (std::is_same_v<T, ContourEdgeKey>)
                return ref.topology == Topology::edge && key.sketch.value() != 0 &&
                       key.element != 0;
            else
                return ref.topology == Topology::face && !key.region.empty() &&
                       key.boundary.sketch.value() != 0 && key.boundary.element != 0;
        },
        ref.origin);
}

// Classification oracle for R02: R03/R04 must produce and validate actual kernel lineage.
// No geometry search, history synthesis, or product resolver is implemented here.
inline Resolution classify(const SubshapeRef& ref, const EvaluationScope& current,
                           const LineageProjection& lineage) {
    if (!validKey(ref) || !validKey(lineage.source) || current.outputFeature.value() == 0)
        throw std::invalid_argument("Invalid reference contract input");
    if (ref.document != current.document || ref.body != current.body || lineage.target != current)
        return {Status::missing, Reason::wrongScope, current, {}};
    if (lineage.source != ref)
        return {Status::missing, Reason::unknownOrigin, current, {}};
    auto candidates = lineage.candidates;
    for (const auto& candidate : candidates)
        if (candidate.token.empty() || candidate.topology != ref.topology)
            throw std::invalid_argument("Invalid candidate topology/token");
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.token < b.token; });
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (lineage.coverage == Coverage::unavailable)
        return {candidates.size() > 1 ? Status::ambiguous : Status::missing,
                Reason::historyUnavailable, current, candidates};
    if (lineage.coverage == Coverage::deleted) {
        if (!candidates.empty())
            throw std::invalid_argument("Deleted lineage cannot retain descendants");
        return {Status::missing, Reason::deleted, current, {}};
    }
    if (candidates.empty())
        throw std::invalid_argument("Empty complete lineage requires explicit deletion or gap");
    if (candidates.size() == 1)
        return {Status::resolved, Reason::unique, current, candidates};
    return {Status::ambiguous, Reason::split, current, candidates};
}
} // namespace voelund::reference_contract
