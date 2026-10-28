// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <stdexcept>
#include <utility>

// M09: a mirror ("Spiegeln") of a body about a base plane, a construction plane or a face.
namespace voelund::document {

namespace {
// The construction plane or face a plane names.
void validatePlaneTarget(const MirrorPlane& plane) {
    if (plane.constructionPlane && plane.constructionPlane->value() == 0)
        throw std::invalid_argument("A mirror names its construction plane");
    if (!plane.face)
        return;
    if (plane.face->body.value() == 0)
        throw std::invalid_argument("A mirror face names its body");
    validateSubshapeOrigin(plane.face->origin);
}
} // namespace

// M09/N12a: exactly one of a base plane, a construction plane or a body face.
void validateMirrorPlane(const MirrorPlane& plane) {
    const int references =
        (plane.basePlane ? 1 : 0) + (plane.constructionPlane ? 1 : 0) + (plane.face ? 1 : 0);
    if (references != 1)
        throw std::invalid_argument(
            "A mirror plane is a base plane, a construction plane or a body face");
    if (plane.basePlane && (*plane.basePlane < DatumPlane::xy || *plane.basePlane > DatumPlane::yz))
        throw std::invalid_argument("Unknown base plane");
    validatePlaneTarget(plane);
}

void validateMirrorParameters(const MirrorParameters& parameters) {
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A mirror names its body");
    validateMirrorPlane(parameters.plane);
    if (parameters.mode != MirrorMode::newBody && parameters.mode != MirrorMode::join)
        throw std::invalid_argument("Unknown mirror mode");
    validateRepeatedSteps(parameters.features, parameters.mode == MirrorMode::join); // N11b
}

FeatureId ModelDocument::addMirrorFeature(std::string name, const MirrorParameters& parameters) {
    validateMirrorParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::mirror, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const MirrorParameters& ModelDocument::mirrorParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<MirrorParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a mirror");
    return *value;
}

bool ModelDocument::setMirrorParameters(FeatureId featureId, const MirrorParameters& parameters) {
    validateMirrorParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<MirrorParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a mirror");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<MirrorParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
