// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <stdexcept>
#include <type_traits>
#include <voelund/document/DocumentTransaction.hpp>

namespace voelund::document {
DocumentTransaction::DocumentTransaction(ModelDocument& target)
    : target_(target),
      candidate_(DocumentSnapshot::capture(target).toDocument(target.documentId())),
      baseRevision_(target.revision()) {
    if (target.impl_->transactionCandidate_)
        throw std::logic_error("Nested document transactions are not supported");
    candidate_.impl_->transactionCandidate_ = true;
}
DocumentTransaction::~DocumentTransaction() = default;
void DocumentTransaction::checkOpenAndCurrent() const {
    if (finished_)
        throw std::logic_error("Document transaction is already finished");
    if (target_.revision() != baseRevision_)
        throw std::logic_error("Document changed while editing a candidate");
}
ModelDocument& DocumentTransaction::edit() {
    checkOpenAndCurrent();
    if (prepared_)
        throw std::logic_error("Prepared candidate is frozen");
    return candidate_;
}
bool DocumentTransaction::prepare() {
    checkOpenAndCurrent();
    if (prepared_) {
        if (candidate_.revision() != preparedRevision_)
            throw std::logic_error("Candidate changed after prepare");
        return static_cast<bool>(after_);
    }
    auto before = target_.currentState();
    auto after = candidate_.currentState();
    if (!(before == after)) {
        target_.checkRevisionAvailable();
        auto prepared = std::make_unique<ModelDocument::Impl>(*target_.impl_);
        const auto replacement = IdentifiedDocumentSnapshot::restore(
            target_.documentId(), DocumentSnapshot::capture(candidate_));
        (void)prepared->native.commit(prepared->native.prepare(replacement));
        after_ = std::move(prepared);
    }
    preparedRevision_ = candidate_.revision();
    prepared_ = true;
    return static_cast<bool>(after_);
}
bool DocumentTransaction::commit() {
    const bool changed = prepare();
    // prepare verifies both revisions even when called for the second time.
    if (changed) {
        target_.impl_.swap(after_);
    }
    finished_ = true;
    after_.reset();
    return changed;
}
void DocumentTransaction::rollback() noexcept {
    finished_ = true;
    after_.reset();
}
} // namespace voelund::document
