#include "ModelDocumentImpl.hpp"
#include "NativeBodyAttributes.hpp"
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
            {view.parameters().begin(), view.parameters().end()}, view.nextFeatureId(),
            view.parameterTable(), view.bodyAttributes()};
}
void ModelDocument::commitState(State state) {
    validateBodyAttributes(state.bodyAttributes, state.features, state.parameters);
    const DocumentSnapshot snapshot(std::move(state.features), std::move(state.parameters),
                                    state.nextFeatureId, std::move(state.parameterTable),
                                    std::move(state.bodyAttributes));
    auto prepared = impl_->native.prepare(IdentifiedDocumentSnapshot::restore(documentId(), snapshot),
                                          !impl_->transactionCandidate_);
    (void)impl_->native.commit(std::move(prepared));
}
const ParameterTable& ModelDocument::parameterTable() const noexcept {
    return impl_->view().parameterTable();
}
bool ModelDocument::setParameterTable(const ParameterTable& table) {
    validateParameterTable(table);
    if (table == parameterTable())
        return false;
    auto candidate = currentState();
    validateParameterBindings(candidate.parameters, table);
    candidate.parameterTable = table;
    commitState(std::move(candidate));
    return true;
}
const BodyAttributeTable& ModelDocument::bodyAttributes() const noexcept {
    return impl_->view().bodyAttributes();
}
bool ModelDocument::renameBody(BodyId body, std::string name) {
    auto candidate = currentState();
    auto attributes = document::bodyAttributes(candidate.bodyAttributes, body.value());
    if (attributes.name == name)
        return false;
    attributes.name = std::move(name);
    setBodyAttributes(candidate.bodyAttributes, body.value(), std::move(attributes));
    commitState(std::move(candidate));
    return true;
}
bool ModelDocument::setBodyHidden(BodyId body, bool hidden) {
    auto candidate = currentState();
    auto attributes = document::bodyAttributes(candidate.bodyAttributes, body.value());
    if (attributes.hidden == hidden)
        return false;
    attributes.hidden = hidden;
    setBodyAttributes(candidate.bodyAttributes, body.value(), std::move(attributes));
    validateBodyAttributes(candidate.bodyAttributes, candidate.features, candidate.parameters);
    // Stored and modified, but no undo step: undo and redo keep the current visibility.
    const DocumentSnapshot snapshot(std::move(candidate.features), std::move(candidate.parameters),
                                    candidate.nextFeatureId, std::move(candidate.parameterTable),
                                    std::move(candidate.bodyAttributes));
    auto prepared = impl_->native.prepareViewChange(
        IdentifiedDocumentSnapshot::restore(documentId(), snapshot));
    return impl_->native.commit(std::move(prepared));
}
bool ModelDocument::canUndo() const noexcept { return impl_->native.canUndo(); }
bool ModelDocument::canRedo() const noexcept { return impl_->native.canRedo(); }
bool ModelDocument::undo() { return impl_->native.commit(impl_->native.prepareUndo()); }
bool ModelDocument::redo() { return impl_->native.commit(impl_->native.prepareRedo()); }
} // namespace voelund::document