// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <voelund/application/CommandTransaction.hpp>

namespace voelund::application {
CommandTransaction::CommandTransaction(CommandKind kind,
                                       std::unique_ptr<CommandTransactionBackend> backend)
    : kind_(kind), backend_(std::move(backend)) {
    if (!backend_)
        throw std::invalid_argument("Command transaction requires a backend");
}
CommandTransaction::~CommandTransaction() { rollback(); }
void CommandTransaction::rollback() noexcept {
    if (!finished_) {
        backend_->rollback();
        finished_ = true;
    }
}
CommandResult CommandTransaction::reject(RejectionReason reason,
                                         features::EvaluationReport report) {
    if (finished_)
        return {CommandStatus::rejected, RejectionReason::alreadyFinished};
    CommandResult result(CommandStatus::rejected, reason, std::move(report));
    rollback();
    return result;
}
CommandResult CommandTransaction::cancel() { return reject(RejectionReason::cancelled); }
CommandResult CommandTransaction::noChange() { return reject(RejectionReason::noChange); }
CommandResult CommandTransaction::rejectInput(features::EvaluationReport diagnostics) {
    return reject(RejectionReason::invalidInput, std::move(diagnostics));
}
CommandResult CommandTransaction::finish(features::EvaluationReport evaluation,
                                         ErrorDisposition disposition) {
    if (finished_)
        return {CommandStatus::rejected, RejectionReason::alreadyFinished};
    const bool incomplete = std::ranges::any_of(evaluation.features, [](const auto& entry) {
        return entry.status == features::EvaluationStatus::dirty;
    });
    if (incomplete)
        return reject(RejectionReason::incompleteEvaluation, std::move(evaluation));
    const bool errors = std::ranges::any_of(evaluation.features, [](const auto& entry) {
        return entry.status == features::EvaluationStatus::failed ||
               entry.status == features::EvaluationStatus::blockedByDependency;
    });
    if (errors &&
        (kind_ == CommandKind::newFeature || disposition != ErrorDisposition::keepEditableErrors))
        return reject(RejectionReason::evaluationFailed, std::move(evaluation));
    CommandResult result(errors ? CommandStatus::committedWithErrors : CommandStatus::committed,
                         RejectionReason::none, std::move(evaluation));
    try {
        backend_->prepareCommit(result.evaluation());
    } catch (...) {
        rollback();
        throw;
    } // Infrastructure/programming errors remain exceptions.
    backend_->commitPrepared();
    finished_ = true;
    static_assert(std::is_nothrow_move_constructible_v<CommandResult>);
    return result;
}
} // namespace voelund::application