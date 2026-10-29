// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

// N13b: construction points ("Bezugspunkt").
namespace voelund::document {

void validateDatumPointParameters(const DatumPointParameters& parameters) {
    if (parameters.position.has_value() == parameters.edge.has_value() ||
        parameters.body.has_value() != parameters.edge.has_value())
        throw std::invalid_argument("A construction point lies at coordinates or on an edge");
    if (parameters.position && !std::ranges::all_of(*parameters.position, [](double value) {
            return kernel::modelCoordinate(value);
        }))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (parameters.body && parameters.body->value() == 0)
        throw std::invalid_argument("A construction point names the body of its edge");
    if (parameters.spot < EdgeSpot::start || parameters.spot > EdgeSpot::centre)
        throw std::invalid_argument("Unknown place on the edge");
}

FeatureId ModelDocument::addDatumPointFeature(std::string name,
                                              const DatumPointParameters& parameters) {
    validateDatumPointParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::datumPoint, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const DatumPointParameters& ModelDocument::datumPointParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DatumPointParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a construction point");
    return *value;
}

bool ModelDocument::setDatumPointParameters(FeatureId featureId,
                                            const DatumPointParameters& parameters) {
    validateDatumPointParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DatumPointParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a construction point");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DatumPointParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
