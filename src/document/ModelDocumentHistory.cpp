#include <voelund/document/IdCapacity.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
void ModelDocument::checkRevisionAvailable() const {
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document revision exhausted");
}
FeatureId ModelDocument::nextFeatureId() const {
    if (!hasIdCapacity(nextFeatureId_, 1))
        throw std::overflow_error("Feature IDs exhausted");
    return FeatureId{nextFeatureId_};
}
ModelDocument::State ModelDocument::currentState() const {
    return {features_, parameters_, nextFeatureId_};
}
void ModelDocument::swapState(State& state) noexcept {
    features_.swap(state.features);
    parameters_.swap(state.parameters);
    // Restoring content must never make a published feature ID available again.
    // Transaction candidates own their counter; rollback cannot consume target IDs.
    const auto previous = nextFeatureId_;
    nextFeatureId_ = std::max(nextFeatureId_, state.nextFeatureId);
    state.nextFeatureId = previous;
}
void ModelDocument::commitState(State state) {
    checkRevisionAvailable();
    if (!transactionCandidate_) {
        auto before = currentState();
        undoHistory_.push_back(std::move(before));
        redoHistory_.clear();
    }
    swapState(state);
    ++revision_;
}
bool ModelDocument::canUndo() const noexcept { return !undoHistory_.empty(); }
bool ModelDocument::canRedo() const noexcept { return !redoHistory_.empty(); }
bool ModelDocument::undo() {
    if (!canUndo())
        return false;
    checkRevisionAvailable();
    auto before = currentState();
    redoHistory_.push_back(std::move(before));
    swapState(undoHistory_.back());
    undoHistory_.pop_back();
    ++revision_;
    return true;
}
bool ModelDocument::redo() {
    if (!canRedo())
        return false;
    checkRevisionAvailable();
    auto before = currentState();
    undoHistory_.push_back(std::move(before));
    swapState(redoHistory_.back());
    redoHistory_.pop_back();
    ++revision_;
    return true;
}
} // namespace voelund::document
