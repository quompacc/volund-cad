// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <stdexcept>
#include <string>
#include <utility>

// N12a: a body split in two ("Teilen") at a base plane, a construction plane or a face; with
// both sides kept, the back side is the split's part, a body of its own.
namespace voelund::document {

void validateSplitParameters(const SplitParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A split names its body");
    validateMirrorPlane(parameters.plane);
    if (parameters.keep < SplitKeep::both || parameters.keep > SplitKeep::back)
        throw std::invalid_argument("A split keeps both sides, the front or the back");
}

void keepSplitPartInStep(std::vector<FeatureDescriptor>& features,
                         std::vector<FeatureParameters>& parameters, std::uint64_t& next,
                         FeatureId split, const SplitParameters& values) {
    std::size_t position = 0;
    while (features[position].id != split)
        ++position;
    const std::size_t after = position + 1;
    const bool hasPart = after < features.size() &&
                         std::holds_alternative<SplitPartParameters>(parameters[after]) &&
                         std::get<SplitPartParameters>(parameters[after]).split == split;
    if (values.keep != SplitKeep::both && hasPart) {
        features.erase(features.begin() + static_cast<std::ptrdiff_t>(after));
        parameters.erase(parameters.begin() + static_cast<std::ptrdiff_t>(after));
    }
    if (values.keep != SplitKeep::both || hasPart)
        return;
    if (!hasIdCapacity(next, 1))
        throw std::invalid_argument("No feature IDs left for the split part");
    const FeatureId id{next++};
    features.insert(features.begin() + static_cast<std::ptrdiff_t>(after),
                    FeatureDescriptor{id, FeatureKind::splitPart,
                                      features[position].name + " Teil 2", std::nullopt});
    parameters.insert(parameters.begin() + static_cast<std::ptrdiff_t>(after),
                      SplitPartParameters{split});
}

FeatureId ModelDocument::addSplitFeature(std::string name, const SplitParameters& parameters) {
    validateSplitParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::split, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    keepSplitPartInStep(candidate.features, candidate.parameters, candidate.nextFeatureId, id,
                        parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const SplitParameters& ModelDocument::splitParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<SplitParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a split");
    return *value;
}

bool ModelDocument::setSplitParameters(FeatureId featureId, const SplitParameters& parameters) {
    validateSplitParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<SplitParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a split");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<SplitParameters>(&candidate.parameters.at(index)) = parameters;
    keepSplitPartInStep(candidate.features, candidate.parameters, candidate.nextFeatureId,
                        featureId, parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
