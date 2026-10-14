#include "ModelDocumentImpl.hpp"

#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <stdexcept>

// Features added on 2026-10-14 (M19-M27): their parameters, validation and document access.
namespace voelund::document {

void validateCombineParameters(const CombineParameters& parameters) {
    if (parameters.target.value() == 0)
        throw std::invalid_argument("A combination names its target body");
    if (parameters.tools.empty() || parameters.tools.size() > maximumCombineTools)
        throw std::invalid_argument("A combination uses between 1 and 64 tool bodies");
    switch (parameters.operation) {
    case CombineOperation::join:
    case CombineOperation::cut:
    case CombineOperation::intersect:
        break;
    default:
        throw std::invalid_argument("Unknown combine operation");
    }
    for (std::size_t index = 0; index < parameters.tools.size(); ++index) {
        const BodyId tool = parameters.tools[index];
        if (tool.value() == 0 || tool == parameters.target)
            throw std::invalid_argument("A tool body is another body than the target");
        if (std::find(parameters.tools.begin(), parameters.tools.begin() + index, tool) !=
            parameters.tools.begin() + index)
            throw std::invalid_argument("Each tool body is named once");
    }
}

FeatureId ModelDocument::addCombineFeature(std::string name,
                                           const CombineParameters& parameters) {
    validateCombineParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::combine, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const CombineParameters& ModelDocument::combineParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<CombineParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a combination");
    return *value;
}

bool ModelDocument::setCombineParameters(FeatureId featureId,
                                         const CombineParameters& parameters) {
    validateCombineParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<CombineParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a combination");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<CombineParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
