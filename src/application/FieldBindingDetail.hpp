// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/FieldBindingRequest.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <optional>
#include <string>
#include <vector>

namespace voelund::application::detail {
// N01b: binds or unbinds one field of `feature` in `candidate`, its value following at once;
// true when that changed the document. Throws std::invalid_argument for an unknown field.
bool applyFieldRequest(document::ModelDocument& candidate, const FieldBindingRequest& request,
                       document::FeatureId feature);
// The shell's pending requests for a feature command: to the feature named or, without one, to
// the feature `candidate` has and `before` has not.
void applyPendingFields(document::ModelDocument& candidate, const document::ModelDocument& before,
                        const std::vector<FieldBindingRequest>& pending);
// Takes the pending requests and applies them; why they could not be, if so.
[[nodiscard]] std::optional<std::string>
takePendingFields(document::ModelDocument& candidate, const document::ModelDocument& before,
                  std::vector<FieldBindingRequest>& pending);
} // namespace voelund::application::detail
