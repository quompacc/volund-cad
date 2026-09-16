#pragma once
#include "NativeModelState.hpp"
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
// Ephemeral edit DTO; never retained as a second mutable model authority.
struct ModelDocument::State final {
    std::vector<FeatureDescriptor> features;
    std::vector<FeatureParameters> parameters;
    std::uint64_t nextFeatureId;
    [[nodiscard]] bool operator==(const State&) const = default;
};
struct ModelDocument::Impl final {
    explicit Impl(const IdentifiedDocumentSnapshot& snapshot) : native(snapshot) {}
    [[nodiscard]] const DocumentSnapshot& view() const noexcept { return native.view().snapshot(); }
    detail::NativeModelState native;
    bool transactionCandidate_{false};
};
} // namespace voelund::document