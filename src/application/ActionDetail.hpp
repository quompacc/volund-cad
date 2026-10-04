#pragma once
#include <voelund/application/Actions.hpp>
#include <voelund/document/DocumentSnapshot.hpp>

#include <vector>

namespace voelund::application::detail {
// The single capability rule shared by the context and the validation of requests.
[[nodiscard]] std::vector<ActionKind> availableActions(const document::DocumentSnapshot& snapshot,
                                                       const ActionTarget& target);
} // namespace voelund::application::detail
