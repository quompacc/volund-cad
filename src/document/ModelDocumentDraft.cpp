#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

// M25: a draft ("Formschräge") tilts named faces of a body about the neutral face's plane.
namespace voelund::document {

void validateDraftParameters(const DraftParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A draft names its body");
    const double angle = parameters.angle.value();
    if (!std::isfinite(angle) || std::abs(angle) <= 1e-9 || std::abs(angle) >= std::numbers::pi / 2)
        throw std::invalid_argument("A draft angle lies between 0 and 90 degrees");
    if (parameters.faces.empty() || parameters.faces.size() > maximumDraftFaces)
        throw std::invalid_argument("A draft tilts between 1 and 256 faces");
    validateSubshapeOrigin(parameters.neutral);
    for (std::size_t index = 0; index < parameters.faces.size(); ++index) {
        validateSubshapeOrigin(parameters.faces[index]);
        if (parameters.faces[index] == parameters.neutral ||
            std::find(parameters.faces.begin(), parameters.faces.begin() + static_cast<std::ptrdiff_t>(index),
                      parameters.faces[index]) != parameters.faces.begin() + static_cast<std::ptrdiff_t>(index))
            throw std::invalid_argument("Each drafted face is named once, apart from the neutral face");
    }
}

FeatureId ModelDocument::addDraftFeature(std::string name, const DraftParameters& parameters) {
    validateDraftParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::draft, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const DraftParameters& ModelDocument::draftParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DraftParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a draft");
    return *value;
}

bool ModelDocument::setDraftParameters(FeatureId featureId, const DraftParameters& parameters) {
    validateDraftParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DraftParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a draft");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DraftParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
