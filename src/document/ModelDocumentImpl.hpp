// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "NativeModelState.hpp"
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
// Ephemeral edit DTO; never retained as a second mutable model authority.
struct ModelDocument::State final {
    std::vector<FeatureDescriptor> features;
    std::vector<FeatureParameters> parameters;
    std::uint64_t nextFeatureId;
    ParameterTable parameterTable{};
    BodyAttributeTable bodyAttributes{};
    std::optional<FeatureId> rollbackAfter{}; // M14
    [[nodiscard]] bool operator==(const State&) const = default;
};
// M10: the copies of `pattern` follow it directly; as new bodies there is one per index
// 1..count-1, joined there is none. Copies that stay keep their ids (and so their bodies).
void validateMirrorPlane(const MirrorPlane& plane); // M09/N12a
// N12a: a split keeping both sides is followed by its part; otherwise it has none.
void keepSplitPartInStep(std::vector<FeatureDescriptor>& features,
                         std::vector<FeatureParameters>& parameters, std::uint64_t& next,
                         FeatureId split, const SplitParameters& values);
void keepCopiesInStep(std::vector<FeatureDescriptor>& features,
                      std::vector<FeatureParameters>& parameters, std::uint64_t& next,
                      FeatureId pattern, const PatternParameters& values);
struct ModelDocument::Impl final {
    explicit Impl(const IdentifiedDocumentSnapshot& snapshot) : native(snapshot) {}
    [[nodiscard]] const DocumentSnapshot& view() const noexcept { return native.view().snapshot(); }
    detail::NativeModelState native;
    bool transactionCandidate_{false};
};
} // namespace voelund::document