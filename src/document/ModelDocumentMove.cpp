#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

// M11: a move ("Bewegen") of a body where it stands in the history.
namespace voelund::document {

void validateMoveParameters(const MoveParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A move names its body");
    const auto& transform = parameters.transform;
    for (const double value : {transform.translationX, transform.translationY,
                               transform.translationZ, parameters.pivot[0], parameters.pivot[1],
                               parameters.pivot[2]})
        if (!kernel::modelCoordinate(value))
            throw std::invalid_argument(kernel::modelRangeDiagnostic);
    for (const double value : {transform.rotationX.value(), transform.rotationY.value(),
                               transform.rotationZ.value()})
        if (!std::isfinite(value))
            throw std::invalid_argument("A move turns by finite angles");
}

FeatureId ModelDocument::addMoveFeature(std::string name, const MoveParameters& parameters) {
    validateMoveParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::move, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const MoveParameters& ModelDocument::moveParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<MoveParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a move");
    return *value;
}

bool ModelDocument::setMoveParameters(FeatureId featureId, const MoveParameters& parameters) {
    validateMoveParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<MoveParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a move");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<MoveParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
