// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/application/Actions.hpp>
#include <voelund/document/DocumentSnapshot.hpp>

#include <vector>

namespace voelund::application::detail {
// The single capability rule shared by the context and the validation of requests.
// Feature targets also depend on the current results (cylinder input, placement).
[[nodiscard]] std::vector<ActionKind> availableActions(const CadSession& session,
                                                       const document::DocumentSnapshot& snapshot,
                                                       const ActionTarget& target);
} // namespace voelund::application::detail
