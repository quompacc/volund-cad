// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <optional>
#include <string>
#include <utility>
#include <voelund/features/EvaluationState.hpp>

namespace voelund::application {
enum class CommandStatus { rejected, committed, committedWithErrors };
enum class RejectionReason {
    none,
    cancelled,
    noChange,
    invalidInput,
    evaluationFailed,
    incompleteEvaluation,
    alreadyFinished
};
class CommandTransaction;
class CommandResult final {
  public:
    [[nodiscard]] CommandStatus status() const noexcept { return status_; }
    [[nodiscard]] RejectionReason reason() const noexcept { return reason_; }
    [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }
    [[nodiscard]] static CommandResult invalidInput(std::string diagnostic) {
        CommandResult result(CommandStatus::rejected, RejectionReason::invalidInput);
        result.diagnostic_ = std::move(diagnostic);
        return result;
    }
    [[nodiscard]] const features::EvaluationReport& evaluation() const noexcept {
        return evaluation_;
    }
    [[nodiscard]] std::optional<document::FeatureId> feature() const noexcept { return feature_; }
    // Deliberately no bool conversion: callers must handle all three outcomes.
  private:
    friend class CommandTransaction;
    friend class CadSession;
    CommandResult(CommandStatus status, RejectionReason reason,
                  features::EvaluationReport evaluation = {})
        : status_(status), reason_(reason), evaluation_(std::move(evaluation)) {}
    CommandStatus status_;
    RejectionReason reason_;
    features::EvaluationReport evaluation_;
    std::optional<document::FeatureId> feature_;
    std::string diagnostic_;
};
} // namespace voelund::application
