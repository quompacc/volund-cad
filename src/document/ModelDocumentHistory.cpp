#include "ModelDocumentImpl.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
void ModelDocument::checkRevisionAvailable() const {
    if (impl_->revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document revision exhausted");
}
FeatureId ModelDocument::nextFeatureId() const {
    if (!hasIdCapacity(impl_->nextFeatureId_, 1))
        throw std::overflow_error("Feature IDs exhausted");
    return FeatureId{impl_->nextFeatureId_};
}
ModelDocument::State ModelDocument::currentState() const {
    return {impl_->features_, impl_->parameters_, impl_->nextFeatureId_};
}
void ModelDocument::swapState(State& state) noexcept {
    impl_->features_.swap(state.features);
    impl_->parameters_.swap(state.parameters);
    // Restoring content must never make a published feature ID available again.
    // Transaction candidates own their counter; rollback cannot consume target IDs.
    const auto previous = impl_->nextFeatureId_;
    impl_->nextFeatureId_ = std::max(impl_->nextFeatureId_, state.nextFeatureId);
    state.nextFeatureId = previous;
}
void ModelDocument::commitState(State state) {
    checkRevisionAvailable();
    if (!impl_->transactionCandidate_) {
        auto before = currentState();
        impl_->undoHistory_.push_back(std::move(before));
        impl_->redoHistory_.clear();
    }
    swapState(state);
    ++impl_->revision_;
}
bool ModelDocument::canUndo() const noexcept { return !impl_->undoHistory_.empty(); }
bool ModelDocument::canRedo() const noexcept { return !impl_->redoHistory_.empty(); }
bool ModelDocument::undo() {
    if (!canUndo())
        return false;
    checkRevisionAvailable();
    auto before = currentState();
    impl_->redoHistory_.push_back(std::move(before));
    swapState(impl_->undoHistory_.back());
    impl_->undoHistory_.pop_back();
    ++impl_->revision_;
    return true;
}
bool ModelDocument::redo() {
    if (!canRedo())
        return false;
    checkRevisionAvailable();
    auto before = currentState();
    impl_->undoHistory_.push_back(std::move(before));
    swapState(impl_->redoHistory_.back());
    impl_->redoHistory_.pop_back();
    ++impl_->revision_;
    return true;
}
} // namespace voelund::document
