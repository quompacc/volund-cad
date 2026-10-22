// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <stdexcept>
#include <utility>

// M27: sphere, cone, torus and wedge as one feature kind with the cylinder's result modes.
namespace voelund::document {

FeatureId ModelDocument::addPrimitiveFeature(std::string name,
                                             std::optional<FeatureId> inputFeature,
                                             const PrimitiveParameters& parameters) {
    if (inputFeature && !contains(*inputFeature))
        throw std::invalid_argument("Primitive input feature does not exist");
    if (!inputFeature && parameters.resultMode != CylinderResultMode::separate)
        throw std::invalid_argument("A primitive without input is a new body");
    validatePrimitiveParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::primitive, std::move(name), inputFeature});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const PrimitiveParameters& ModelDocument::primitiveParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<PrimitiveParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a primitive");
    return *value;
}

bool ModelDocument::setPrimitiveParameters(FeatureId featureId,
                                           const PrimitiveParameters& parameters) {
    validatePrimitiveParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<PrimitiveParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a primitive");
    if (*current == parameters)
        return false;
    if (current->shape != parameters.shape)
        throw std::invalid_argument("A primitive keeps its shape");
    if (!impl_->view().features()[index].inputFeature &&
        parameters.resultMode != CylinderResultMode::separate)
        throw std::invalid_argument("A primitive without input is a new body");
    auto candidate = currentState();
    *std::get_if<PrimitiveParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
