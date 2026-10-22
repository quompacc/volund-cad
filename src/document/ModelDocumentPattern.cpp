// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

// M10: linear and circular patterns of a body ("Muster"); as new bodies with one copy
// feature per copy, which the document keeps in step with the pattern.
namespace voelund::document {

void keepCopiesInStep(std::vector<FeatureDescriptor>& features,
                      std::vector<FeatureParameters>& parameters, std::uint64_t& next,
                      FeatureId pattern, const PatternParameters& values) {
    const auto copyOf = [&](std::size_t index) {
        const auto* copy = std::get_if<PatternCopyParameters>(&parameters[index]);
        return copy != nullptr && copy->pattern == pattern ? copy : nullptr;
    };
    const std::uint32_t wanted = values.mode == PatternMode::newBodies ? values.count - 1 : 0;
    for (std::size_t index = features.size(); index-- > 0;)
        if (const auto* copy = copyOf(index); copy != nullptr && copy->index > wanted) {
            features.erase(features.begin() + static_cast<std::ptrdiff_t>(index));
            parameters.erase(parameters.begin() + static_cast<std::ptrdiff_t>(index));
        }
    std::size_t position = 0;
    while (features[position].id != pattern)
        ++position;
    const std::string name = features[position].name;
    for (std::uint32_t copy = 1; copy <= wanted; ++copy) {
        ++position;
        if (position < features.size() && copyOf(position) != nullptr &&
            copyOf(position)->index == copy)
            continue;
        if (!hasIdCapacity(next, 1))
            throw std::invalid_argument("No feature IDs left for the pattern copies");
        const FeatureId id{next++};
        features.insert(features.begin() + static_cast<std::ptrdiff_t>(position),
                        FeatureDescriptor{id, FeatureKind::patternCopy,
                                          name + " Kopie " + std::to_string(copy), std::nullopt});
        parameters.insert(parameters.begin() + static_cast<std::ptrdiff_t>(position),
                          PatternCopyParameters{pattern, copy});
    }
}

void validatePatternParameters(const PatternParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A pattern names its body");
    const int references = (parameters.axis.baseAxis ? 1 : 0) + (parameters.axis.edge ? 1 : 0) +
                           (parameters.datumAxis ? 1 : 0);
    if (references != 1)
        throw std::invalid_argument(
            "A pattern runs along a base axis, a body edge or a construction axis");
    if (parameters.datumAxis) {
        if (parameters.datumAxis->value() == 0 || parameters.axis != DatumAxisParameters{})
            throw std::invalid_argument("A pattern names its construction axis");
    } else {
        validateDatumAxisParameters(parameters.axis);
    }
    if (parameters.kind != PatternKind::linear && parameters.kind != PatternKind::circular)
        throw std::invalid_argument("Unknown pattern kind");
    if (parameters.mode != PatternMode::newBodies && parameters.mode != PatternMode::join)
        throw std::invalid_argument("Unknown pattern result");
    if (parameters.count < 2 || parameters.count > maximumPatternCount)
        throw std::invalid_argument("A pattern has 2 to 100 instances");
    const double angle = parameters.angle.value();
    if (!std::isfinite(parameters.spacing) || !std::isfinite(angle) ||
        std::abs(parameters.spacing) > 1e6 || angle < 0.0 || angle > 2 * std::numbers::pi + 1e-12)
        throw std::invalid_argument("Pattern spacing or angle out of range");
    if (parameters.kind == PatternKind::linear && parameters.spacing <= 1e-9)
        throw std::invalid_argument("A linear pattern needs a positive spacing");
    if (parameters.kind == PatternKind::circular && angle <= 1e-9)
        throw std::invalid_argument("A circular pattern needs a positive angle");
}

FeatureId ModelDocument::addPatternFeature(std::string name, const PatternParameters& parameters) {
    validatePatternParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::pattern, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    keepCopiesInStep(candidate.features, candidate.parameters, candidate.nextFeatureId, id,
                     parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const PatternParameters& ModelDocument::patternParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<PatternParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a pattern");
    return *value;
}

bool ModelDocument::setPatternParameters(FeatureId featureId, const PatternParameters& parameters) {
    validatePatternParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<PatternParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a pattern");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<PatternParameters>(&candidate.parameters.at(index)) = parameters;
    keepCopiesInStep(candidate.features, candidate.parameters, candidate.nextFeatureId, featureId,
                     parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
