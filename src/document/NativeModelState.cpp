#include "NativeModelState.hpp"
#include "NativeBodyAttributes.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace voelund::document::detail {
Handle(TDocStd_Document) NativeModelState::encode(const IdentifiedDocumentSnapshot& value) {
    Handle(TDocStd_Document) native = new TDocStd_Document("BinOcaf");
    NativePrimitiveCodec::write(native, value);
    if (!(NativePrimitiveCodec::read(native) == value))
        throw std::logic_error("Native model state failed snapshot parity");
    return native;
}
NativeModelState::NativeModelState(const IdentifiedDocumentSnapshot& initial)
    : state_(std::make_unique<State>()) {
    state_->current = encode(initial);
    state_->view = std::make_shared<const IdentifiedDocumentSnapshot>(snapshot());
}
NativeModelState::NativeModelState(const NativeModelState& other)
    : state_(std::make_unique<State>(*other.state_)) {}
IdentifiedDocumentSnapshot NativeModelState::snapshot() const {
    return NativePrimitiveCodec::read(state_->current);
}
std::uint64_t NativeModelState::revision() const noexcept { return state_->revision; }
bool NativeModelState::canUndo() const noexcept { return !state_->undo.empty(); }
bool NativeModelState::canRedo() const noexcept { return !state_->redo.empty(); }
void NativeModelState::checkRevisionAvailable() const {
    if (revision() == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Native model revision exhausted");
}
NativeModelState::Prepared NativeModelState::prepare(
        const IdentifiedDocumentSnapshot& replacement, bool recordHistory) const {
    const auto& before = view();
    if (replacement.id() != before.id() ||
        replacement.snapshot().nextFeatureId() < before.snapshot().nextFeatureId())
        throw std::invalid_argument("Native revision cannot change identity or lower ID watermark");
    if (replacement == before)
        return Prepared(this, revision(), nullptr);
    checkRevisionAvailable();
    auto next = std::make_unique<State>(*state_);
    next->current = encode(replacement);
    next->view = std::make_shared<const IdentifiedDocumentSnapshot>(NativePrimitiveCodec::read(next->current));
    if (recordHistory) next->undo.push_back(state_->current);
    next->redo.clear();
    ++next->revision;
    return Prepared(this, revision(), std::move(next));
}
NativeModelState::Prepared NativeModelState::prepareHistory(bool undo) const {
    const auto& history = undo ? state_->undo : state_->redo;
    if (history.empty())
        return Prepared(this, revision(), nullptr);
    checkRevisionAvailable();
    const auto& before = view();
    const auto historical = NativePrimitiveCodec::read(history.back());
    auto content = historical.snapshot();
    // Undo restores content, never the old published-ID allocation watermark.
    content.nextFeatureId_ = std::max(content.nextFeatureId_, before.snapshot().nextFeatureId());
    content.parameterTable_.nextId =
        std::max(content.parameterTable_.nextId, before.snapshot().parameterTable().nextId);
    // M01: undo and redo restore names, never the visibility the user set since.
    {
        BodyAttributeTable carried;
        for (auto entry : content.bodyAttributes_) {
            entry.attributes.hidden = false;
            setBodyAttributes(carried, entry.body, std::move(entry.attributes));
        }
        for (const auto& entry : before.snapshot().bodyAttributes())
            if (entry.attributes.hidden &&
                std::ranges::find(content.features_, FeatureId{entry.body},
                                  &FeatureDescriptor::id) != content.features_.end()) {
                auto attributes = bodyAttributes(carried, entry.body);
                attributes.hidden = true;
                setBodyAttributes(carried, entry.body, std::move(attributes));
            }
        try {
            validateBodyAttributes(carried, content.features_, content.parameters_);
        } catch (const std::invalid_argument&) {
            // A carried visibility on a feature that no longer creates a body: names only.
            BodyAttributeTable names;
            for (auto entry : carried) {
                entry.attributes.hidden = false;
                setBodyAttributes(names, entry.body, std::move(entry.attributes));
            }
            carried = std::move(names);
        }
        content.bodyAttributes_ = std::move(carried);
    }
    // M14: undo and redo keep the rollback marker where the user put it, while its feature is
    // still there (and not the last one).
    content.rollbackAfter_ = before.snapshot().rollbackAfter();
    if (content.rollbackAfter_ &&
        (std::ranges::find(content.features_, *content.rollbackAfter_, &FeatureDescriptor::id) ==
             content.features_.end() ||
         content.features_.back().id == *content.rollbackAfter_))
        content.rollbackAfter_.reset();
    const auto restored = IdentifiedDocumentSnapshot::restore(before.id(), std::move(content));
    auto next = std::make_unique<State>(*state_);
    next->current = encode(restored);
    next->view = std::make_shared<const IdentifiedDocumentSnapshot>(NativePrimitiveCodec::read(next->current));
    (undo ? next->redo : next->undo).push_back(state_->current);
    (undo ? next->undo : next->redo).pop_back();
    ++next->revision;
    return Prepared(this, revision(), std::move(next));
}
NativeModelState::Prepared
NativeModelState::prepareViewChange(const IdentifiedDocumentSnapshot& replacement) const {
    const auto& before = view();
    if (replacement.id() != before.id() ||
        replacement.snapshot().nextFeatureId() != before.snapshot().nextFeatureId())
        throw std::invalid_argument("A view change keeps identity and IDs");
    if (replacement == before)
        return Prepared(this, revision(), nullptr);
    checkRevisionAvailable();
    auto next = std::make_unique<State>(*state_);
    next->current = encode(replacement);
    next->view = std::make_shared<const IdentifiedDocumentSnapshot>(NativePrimitiveCodec::read(next->current));
    ++next->revision;
    return Prepared(this, revision(), std::move(next));
}
NativeModelState::Prepared NativeModelState::prepareUndo() const { return prepareHistory(true); }
NativeModelState::Prepared NativeModelState::prepareRedo() const { return prepareHistory(false); }
bool NativeModelState::commit(Prepared&& prepared) {
    if (prepared.consumed_ || prepared.owner_ != this || prepared.revision_ != revision())
        throw std::logic_error("Native model change is consumed, foreign or stale");
    const bool changed = static_cast<bool>(prepared.state_);
    if (changed) state_.swap(prepared.state_);
    prepared.consumed_ = true;
    return changed;
}
} // namespace voelund::document::detail
