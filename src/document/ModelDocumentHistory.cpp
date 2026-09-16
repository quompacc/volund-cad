#include "ModelDocumentImpl.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <limits>
#include <stdexcept>

namespace voelund::document {
void ModelDocument::checkRevisionAvailable() const {
    if (revision() == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document revision exhausted");
}
FeatureId ModelDocument::nextFeatureId() const {
    const auto next = impl_->view().nextFeatureId();
    if (!hasIdCapacity(next, 1)) throw std::overflow_error("Feature IDs exhausted");
    return FeatureId{next};
}
ModelDocument::State ModelDocument::currentState() const {
    const auto& view = impl_->view();
    return {{view.features().begin(), view.features().end()},
            {view.parameters().begin(), view.parameters().end()}, view.nextFeatureId()};
}
void ModelDocument::commitState(State state) {
    const DocumentSnapshot snapshot(std::move(state.features), std::move(state.parameters),
                                    state.nextFeatureId);
    auto prepared = impl_->native.prepare(IdentifiedDocumentSnapshot::restore(documentId(), snapshot),
                                          !impl_->transactionCandidate_);
    (void)impl_->native.commit(std::move(prepared));
}
bool ModelDocument::canUndo() const noexcept { return impl_->native.canUndo(); }
bool ModelDocument::canRedo() const noexcept { return impl_->native.canRedo(); }
bool ModelDocument::undo() { return impl_->native.commit(impl_->native.prepareUndo()); }
bool ModelDocument::redo() { return impl_->native.commit(impl_->native.prepareRedo()); }
} // namespace voelund::document