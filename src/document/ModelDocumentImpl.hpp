#pragma once
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
struct ModelDocument::State final {
    std::vector<FeatureDescriptor> features;
    std::vector<FeatureParameters> parameters;
    std::uint64_t nextFeatureId;
    [[nodiscard]] bool operator==(const State&) const = default;
};

// Sole mutable model authority. OCAF replaces this storage in the next D12 slice.
struct ModelDocument::Impl final {
    std::vector<FeatureDescriptor> features_;
    std::vector<FeatureParameters> parameters_;
    std::uint64_t nextFeatureId_{1};
    std::vector<State> undoHistory_;
    std::vector<State> redoHistory_;
    std::uint64_t revision_{0};
    bool transactionCandidate_{false};
};
} // namespace voelund::document