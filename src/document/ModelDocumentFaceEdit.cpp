// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

// N13a: faces of a body moved along their normals or removed ("Flächen bearbeiten").
namespace voelund::document {

namespace {
void validateFaces(const std::vector<SubshapeOrigin>& faces) {
    if (faces.empty() || faces.size() > maximumEditedFaces)
        throw std::invalid_argument("A face edit names between 1 and 256 faces");
    for (std::size_t index = 0; index < faces.size(); ++index) {
        validateSubshapeOrigin(faces[index]);
        if (std::find(faces.begin(), faces.begin() + static_cast<std::ptrdiff_t>(index),
                      faces[index]) != faces.begin() + static_cast<std::ptrdiff_t>(index))
            throw std::invalid_argument("Each edited face is named once");
    }
}
} // namespace

void validateFaceEditParameters(const FaceEditParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A face edit names its body");
    if (parameters.mode != FaceEditMode::offset && parameters.mode != FaceEditMode::remove)
        throw std::invalid_argument("A face edit moves or removes faces");
    if (parameters.mode == FaceEditMode::offset &&
        (!kernel::modelCoordinate(parameters.distance) || std::abs(parameters.distance) < 1e-6))
        throw std::invalid_argument("Faces move by a distance other than 0");
    validateFaces(parameters.faces);
}

FeatureId ModelDocument::addFaceEditFeature(std::string name,
                                            const FaceEditParameters& parameters) {
    validateFaceEditParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::faceEdit, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const FaceEditParameters& ModelDocument::faceEditParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<FaceEditParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a face edit");
    return *value;
}

bool ModelDocument::setFaceEditParameters(FeatureId featureId,
                                          const FaceEditParameters& parameters) {
    validateFaceEditParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<FaceEditParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a face edit");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<FaceEditParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
