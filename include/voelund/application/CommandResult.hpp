#pragma once
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
    [[nodiscard]] const features::EvaluationReport& evaluation() const noexcept {
        return evaluation_;
    }
    // Deliberately no bool conversion: callers must handle all three outcomes.
  private:
    friend class CommandTransaction;
    CommandResult(CommandStatus status, RejectionReason reason,
                  features::EvaluationReport evaluation = {})
        : status_(status), reason_(reason), evaluation_(std::move(evaluation)) {}
    CommandStatus status_;
    RejectionReason reason_;
    features::EvaluationReport evaluation_;
};
} // namespace voelund::application