// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include "NativeBodyAttributes.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <voelund/document/FeatureFields.hpp>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document {
void ModelDocument::checkRevisionAvailable() const {
    if (revision() == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document revision exhausted");
}
FeatureId ModelDocument::nextFeatureId() const {
    const auto next = impl_->view().nextFeatureId();
    if (!hasIdCapacity(next, 1))
        throw std::overflow_error("Feature IDs exhausted");
    return FeatureId{next};
}
ModelDocument::State ModelDocument::currentState() const {
    const auto& view = impl_->view();
    return {{view.features().begin(), view.features().end()},
            {view.parameters().begin(), view.parameters().end()},
            view.nextFeatureId(),
            view.parameterTable(),
            view.bodyAttributes(),
            view.rollbackAfter()};
}
namespace {
// M14: features new in `state` stand at the rollback marker, which then follows the last of
// them; a marker on a feature that is gone, or after the last one, means the end.
template <class State>
void placeAtRollback(State& state, std::span<const FeatureDescriptor> before) {
    const auto known = [&](FeatureId id) {
        return std::ranges::find(before, id, &FeatureDescriptor::id) != before.end();
    };
    if (state.rollbackAfter) {
        const auto marker =
            std::ranges::find(state.features, *state.rollbackAfter, &FeatureDescriptor::id);
        if (marker == state.features.end()) {
            state.rollbackAfter.reset();
        } else {
            std::vector<FeatureDescriptor> added;
            std::vector<FeatureParameters> addedParameters;
            std::vector<FeatureDescriptor> kept;
            std::vector<FeatureParameters> keptParameters;
            for (std::size_t index = 0; index < state.features.size(); ++index) {
                auto& target = known(state.features[index].id) ? kept : added;
                auto& values = known(state.features[index].id) ? keptParameters : addedParameters;
                target.push_back(std::move(state.features[index]));
                values.push_back(std::move(state.parameters[index]));
            }
            if (!added.empty()) {
                auto at = std::ranges::find(kept, *state.rollbackAfter, &FeatureDescriptor::id) -
                          kept.begin() + 1;
                // M10/M17: never between a pattern and its copies or an import and its parts.
                while (at < std::ssize(kept) &&
                       isCompanion(kept[static_cast<std::size_t>(at)].kind))
                    ++at;
                const auto count = static_cast<std::ptrdiff_t>(added.size());
                state.rollbackAfter = added.back().id;
                kept.insert(kept.begin() + at, std::make_move_iterator(added.begin()),
                            std::make_move_iterator(added.end()));
                keptParameters.insert(keptParameters.begin() + at,
                                      std::make_move_iterator(addedParameters.begin()),
                                      std::make_move_iterator(addedParameters.end()));
                // A later feature that took a body as the marker left it (its input names
                // that body's producer before the marker) now takes it after the inserted
                // features: stored, not guessed during evaluation.
                const auto contexts = featureBodyContexts(kept, keptParameters);
                for (std::ptrdiff_t index = at; index < at + count; ++index) {
                    const auto body = contexts[static_cast<std::size_t>(index)];
                    if (!body || kept[static_cast<std::size_t>(index)].kind == FeatureKind::sketch)
                        continue;
                    std::optional<FeatureId> previous;
                    for (std::ptrdiff_t earlier = at; earlier-- > 0;)
                        if (contexts[static_cast<std::size_t>(earlier)] == body &&
                            kept[static_cast<std::size_t>(earlier)].kind != FeatureKind::sketch) {
                            previous = kept[static_cast<std::size_t>(earlier)].id;
                            break;
                        }
                    if (!previous)
                        continue;
                    const auto after = kept[static_cast<std::size_t>(index)].id;
                    for (auto later = static_cast<std::size_t>(at + count); later < kept.size();
                         ++later)
                        if (kept[later].inputFeature == previous)
                            kept[later].inputFeature = after;
                }
            }
            state.features = std::move(kept);
            state.parameters = std::move(keptParameters);
            static_cast<void>(featureBodyContexts(state.features, state.parameters));
        }
    }
    if (state.rollbackAfter && !state.features.empty() &&
        state.features.back().id == *state.rollbackAfter)
        state.rollbackAfter.reset();
}
} // namespace
void ModelDocument::commitState(State state) {
    placeAtRollback(state, impl_->view().features());
    // N01b: bound fields follow their parameters; bindings of what is gone go with it.
    applyFieldBindings(state.parameterTable, state.features, state.parameters,
                       &impl_->view().parameterTable());
    for (const auto& binding : state.parameterTable.bindings) // a bound count changes the copies
        for (std::size_t index = 0; index < state.features.size(); ++index)
            if (const auto* pattern = std::get_if<PatternParameters>(&state.parameters[index]);
                pattern != nullptr && binding.field == "count" &&
                state.features[index].id == binding.feature) {
                const auto values = *pattern;
                keepCopiesInStep(state.features, state.parameters, state.nextFeatureId,
                                 binding.feature, values);
                break;
            }
    validateBodyAttributes(state.bodyAttributes, state.features, state.parameters);
    const DocumentSnapshot snapshot(std::move(state.features), std::move(state.parameters),
                                    state.nextFeatureId, std::move(state.parameterTable),
                                    std::move(state.bodyAttributes), state.rollbackAfter);
    auto prepared = impl_->native.prepare(
        IdentifiedDocumentSnapshot::restore(documentId(), snapshot), !impl_->transactionCandidate_);
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
                                    std::move(candidate.bodyAttributes), candidate.rollbackAfter);
    auto prepared = impl_->native.prepareViewChange(
        IdentifiedDocumentSnapshot::restore(documentId(), snapshot));
    return impl_->native.commit(std::move(prepared));
}
bool ModelDocument::setFeatureSuppressed(FeatureId featureId, bool suppressed) {
    const std::size_t index = featureIndex(featureId);
    auto candidate = currentState();
    if (candidate.features[index].suppressed == suppressed)
        return false;
    candidate.features[index].suppressed = suppressed;
    // M10/M17: a pattern's copies and an import's parts are suppressed with it.
    for (std::size_t other = 0; other < candidate.features.size(); ++other)
        if (companionOwner(candidate.parameters[other]) == featureId)
            candidate.features[other].suppressed = suppressed;
    commitState(std::move(candidate));
    return true;
}
bool ModelDocument::setRollback(std::optional<FeatureId> featureId) {
    auto candidate = currentState();
    if (featureId) {
        static_cast<void>(featureIndex(*featureId)); // throws for an unknown feature
        if (candidate.features.back().id == *featureId)
            featureId.reset();
    }
    if (candidate.rollbackAfter == featureId)
        return false;
    candidate.rollbackAfter = featureId;
    // Stored, but no undo step: undo and redo keep the marker where the user put it.
    const DocumentSnapshot snapshot(std::move(candidate.features), std::move(candidate.parameters),
                                    candidate.nextFeatureId, std::move(candidate.parameterTable),
                                    std::move(candidate.bodyAttributes), candidate.rollbackAfter);
    auto prepared = impl_->native.prepareViewChange(
        IdentifiedDocumentSnapshot::restore(documentId(), snapshot));
    return impl_->native.commit(std::move(prepared));
}
std::optional<FeatureId> ModelDocument::rollbackAfter() const noexcept {
    return impl_->view().rollbackAfter();
}
bool ModelDocument::canUndo() const noexcept { return impl_->native.canUndo(); }
bool ModelDocument::canRedo() const noexcept { return impl_->native.canRedo(); }
bool ModelDocument::undo() { return impl_->native.commit(impl_->native.prepareUndo()); }
bool ModelDocument::redo() { return impl_->native.commit(impl_->native.prepareRedo()); }
} // namespace voelund::document