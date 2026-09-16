#include "ModelDocumentImpl.hpp"
#include <stdexcept>
#include <type_traits>
#include <voelund/document/DocumentTransaction.hpp>

namespace voelund::document {
DocumentTransaction::DocumentTransaction(ModelDocument& target)
    : target_(target), candidate_(ModelDocument::empty()), baseRevision_(target.revision()) {
    if (target.impl_->transactionCandidate_)
        throw std::logic_error("Nested document transactions are not supported");
    auto state = target.currentState();
    candidate_.swapState(state);
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
        target_.impl_->undoHistory_.reserve(target_.impl_->undoHistory_.size() + 1);
        before_ = std::make_unique<ModelDocument::State>(std::move(before));
        after_ = std::make_unique<ModelDocument::State>(std::move(after));
    }
    preparedRevision_ = candidate_.revision();
    prepared_ = true;
    return static_cast<bool>(after_);
}
bool DocumentTransaction::commit() {
    const bool changed = prepare();
    // prepare verifies both revisions even when called for the second time.
    if (changed) {
        static_assert(std::is_nothrow_move_constructible_v<ModelDocument::State>);
        target_.impl_->undoHistory_.push_back(std::move(*before_)); // capacity reserved in prepare
        target_.swapState(*after_);
        target_.impl_->redoHistory_.clear();
        ++target_.impl_->revision_;
    }
    finished_ = true;
    before_.reset();
    after_.reset();
    return changed;
}
void DocumentTransaction::rollback() noexcept {
    finished_ = true;
    before_.reset();
    after_.reset();
}
} // namespace voelund::document