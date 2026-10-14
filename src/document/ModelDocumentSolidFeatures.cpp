#include "ModelDocumentImpl.hpp"

#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
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

void validateDatumPlaneParameters(const DatumPlaneParameters& parameters) {
    if (parameters.basePlane.has_value() == parameters.face.has_value())
        throw std::invalid_argument("A construction plane has one reference");
    if (parameters.basePlane && (static_cast<int>(*parameters.basePlane) < 1 ||
                                 static_cast<int>(*parameters.basePlane) > 3))
        throw std::invalid_argument("Unknown base plane");
    if (parameters.face) {
        if (parameters.face->body.value() == 0)
            throw std::invalid_argument("A construction plane names the body of its face");
        validateSubshapeOrigin(parameters.face->origin);
    }
    if (!kernel::modelCoordinate(parameters.offset))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    switch (parameters.rotationAxis) {
    case DatumRotationAxis::none:
        if (parameters.angle.value() != 0.0)
            throw std::invalid_argument("An angle needs a rotation axis");
        break;
    case DatumRotationAxis::x:
    case DatumRotationAxis::y:
        if (std::abs(parameters.angle.value()) > std::numbers::pi)
            throw std::invalid_argument("The plane turns by at most 180 degrees");
        break;
    default:
        throw std::invalid_argument("Unknown rotation axis");
    }
}

void validateDatumAxisParameters(const DatumAxisParameters& parameters) {
    if (parameters.baseAxis.has_value() == parameters.edge.has_value() ||
        parameters.body.has_value() != parameters.edge.has_value())
        throw std::invalid_argument("A construction axis has one reference");
    if (parameters.baseAxis &&
        (static_cast<int>(*parameters.baseAxis) < 1 || static_cast<int>(*parameters.baseAxis) > 3))
        throw std::invalid_argument("Unknown base axis");
    if (parameters.edge) {
        if (parameters.body->value() == 0 || parameters.edge->first == parameters.edge->second)
            throw std::invalid_argument("A construction axis names a body edge");
        validateSubshapeOrigin(parameters.edge->first);
        validateSubshapeOrigin(parameters.edge->second);
    }
}

FeatureId ModelDocument::addDatumPlaneFeature(std::string name,
                                              const DatumPlaneParameters& parameters) {
    validateDatumPlaneParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::datumPlane, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addDatumAxisFeature(std::string name,
                                             const DatumAxisParameters& parameters) {
    validateDatumAxisParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::datumAxis, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const DatumPlaneParameters& ModelDocument::datumPlaneParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DatumPlaneParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a construction plane");
    return *value;
}

bool ModelDocument::setDatumPlaneParameters(FeatureId featureId,
                                            const DatumPlaneParameters& parameters) {
    validateDatumPlaneParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DatumPlaneParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a construction plane");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DatumPlaneParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const DatumAxisParameters& ModelDocument::datumAxisParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DatumAxisParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a construction axis");
    return *value;
}

bool ModelDocument::setDatumAxisParameters(FeatureId featureId,
                                           const DatumAxisParameters& parameters) {
    validateDatumAxisParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DatumAxisParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a construction axis");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DatumAxisParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
