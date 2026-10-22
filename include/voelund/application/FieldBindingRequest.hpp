// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/document/ParameterId.hpp>

#include <optional>
#include <string>

namespace voelund::application {

// N01b: a field of a feature to bind to `parameter` (nullopt: to unbind, keeping its value).
// Without `feature` it is the feature the next command creates.
struct FieldBindingRequest final {
    std::optional<document::FeatureId> feature;
    std::string field;
    std::optional<document::ParameterId> parameter;

    [[nodiscard]] bool operator==(const FieldBindingRequest&) const = default;
};

} // namespace voelund::application
