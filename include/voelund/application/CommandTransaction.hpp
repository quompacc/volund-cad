// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <memory>
#include <voelund/application/CommandResult.hpp>

namespace voelund::application {
enum class CommandKind { newFeature, editFeature };
enum class ErrorDisposition { reject, keepEditableErrors };

// D05 implements this boundary using the sole authoritative ModelDocument.
// Owns an isolated candidate and rollback data, never Qt objects.
class CommandTransactionBackend {
  public:
    virtual ~CommandTransactionBackend() = default;
    // Allocate history/publication storage here; do not modify the active document.
    virtual void prepareCommit(const features::EvaluationReport& evaluation) = 0;
    // Publish parameters, IDs, history, selection and evaluation together without allocation.
    virtual void commitPrepared() noexcept = 0;
    // Discard candidate; restore all transient editing state. Idempotent, no allocation.
    virtual void rollback() noexcept = 0;
};

class CommandTransaction final {
  public:
    CommandTransaction(CommandKind kind, std::unique_ptr<CommandTransactionBackend> backend);
    ~CommandTransaction();
    CommandTransaction(const CommandTransaction&) = delete;
    CommandTransaction& operator=(const CommandTransaction&) = delete;
    CommandTransaction(CommandTransaction&&) = delete;
    CommandTransaction& operator=(CommandTransaction&&) = delete;
    [[nodiscard]] CommandResult finish(features::EvaluationReport evaluation,
                                       ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult cancel();
    [[nodiscard]] CommandResult rejectInput(features::EvaluationReport diagnostics);
    [[nodiscard]] CommandResult noChange();

  private:
    [[nodiscard]] CommandResult reject(RejectionReason reason,
                                       features::EvaluationReport report = {});
    void rollback() noexcept;
    CommandKind kind_;
    std::unique_ptr<CommandTransactionBackend> backend_;
    bool finished_{false};
};
} // namespace voelund::application