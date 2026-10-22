// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include <string>

// M27 (user decision 2026-10-14): sphere, cone, torus and wedge, added and changed like the
// cylinder. A primitive beside its input keeps its place when the input changes (the placement
// carries the offset between current and reference input), as a separate cylinder does.
namespace voelund::application {
namespace {

using kernel::PrimitiveShape;

const char* shapeName(PrimitiveShape shape) {
    switch (shape) {
    case PrimitiveShape::sphere:
        return "Kugel";
    case PrimitiveShape::cone:
        return "Kegel";
    case PrimitiveShape::torus:
        return "Torus";
    case PrimitiveShape::wedge:
        return "Keil";
    }
    return "Grundkörper";
}

std::array<double, 4> defaultSizes(PrimitiveShape shape) {
    switch (shape) {
    case PrimitiveShape::sphere:
        return {10.0, 0.0, 0.0, 0.0};
    case PrimitiveShape::cone:
        return {10.0, 4.0, 20.0, 0.0};
    case PrimitiveShape::torus:
        return {15.0, 4.0, 0.0, 0.0};
    case PrimitiveShape::wedge:
        return {20.0, 20.0, 20.0, 10.0};
    }
    return {};
}

void shiftBetweenInputAndReference(document::PrimitiveParameters& parameters,
                                   const features::EvaluatedFeature& input, double direction) {
    const double height = kernel::primitiveHeight(parameters.definition());
    const gp_Pnt current = features::CylinderFeature::attachmentOrigin(input.shape, height);
    const gp_Pnt reference =
        features::CylinderFeature::attachmentOrigin(input.referenceShape, height);
    parameters.placement.translationX += direction * (current.X() - reference.X());
    parameters.placement.translationY += direction * (current.Y() - reference.Y());
    parameters.placement.translationZ += direction * (current.Z() - reference.Z());
}

} // namespace

const char* CadSession::primitiveShapeName(PrimitiveShape shape) noexcept {
    return shapeName(shape);
}

CommandResult CadSession::createPrimitive(PrimitiveShape shape,
                                          std::optional<document::FeatureId> input,
                                          document::CylinderResultMode mode) {
    if (input && !canAddCylinder(*input))
        return CommandResult::invalidInput("A primitive on a body needs its current result");
    if (!input && mode != document::CylinderResultMode::separate)
        return CommandResult::invalidInput("A primitive without input is a new body");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    std::size_t sequence = 1;
    for (const auto& feature : document_.features())
        if (feature.kind == document::FeatureKind::primitive &&
            document_.primitiveParameters(feature.id).shape == shape)
            ++sequence;
    document::PrimitiveParameters parameters{
        shape, defaultSizes(shape), kernel::RigidTransform{0.0, 0.0, 0.0, {}, {}, {}}, mode};
    if (input && mode == document::CylinderResultMode::separate)
        shiftBetweenInputAndReference(
            parameters, detail::evaluatedFeature(evaluation_->result.features, *input), 1.0);
    const auto id = detail::prepareInput([&] {
        return candidate.addPrimitiveFeature(
            std::string(shapeName(shape)) + " " + std::to_string(sequence), input, parameters);
    });
    if (!id.value)
        return CommandResult::invalidInput(id.diagnostic);
    return finishCandidate(transaction, *id.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editPrimitiveParameters(document::FeatureId featureId,
                                                  const document::PrimitiveParameters& parameters,
                                                  ErrorDisposition disposition) {
    const auto validation = detail::prepareInput([&] {
        document::validatePrimitiveParameters(parameters);
        return true;
    });
    if (!validation.value)
        return CommandResult::invalidInput(validation.diagnostic);
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::primitive)
        return CommandResult::invalidInput("Feature is not a primitive");
    if (!descriptor->inputFeature &&
        parameters.resultMode != document::CylinderResultMode::separate)
        return CommandResult::invalidInput(
            "A new-body primitive has no body to join, cut or intersect");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    auto adjusted = parameters;
    const auto& current = document_.primitiveParameters(featureId);
    const bool wasSeparate = current.resultMode == document::CylinderResultMode::separate;
    const bool becomesSeparate = adjusted.resultMode == document::CylinderResultMode::separate;
    if (wasSeparate != becomesSeparate && descriptor->inputFeature) {
        if (!featureShape(*descriptor->inputFeature))
            return CommandResult::invalidInput(
                "Repair current geometry before changing the connection mode");
        shiftBetweenInputAndReference(
            adjusted,
            detail::evaluatedFeature(evaluation_->result.features, *descriptor->inputFeature),
            becomesSeparate ? 1.0 : -1.0);
    }
    const auto changed =
        detail::prepareInput([&] { return candidate.setPrimitiveParameters(featureId, adjusted); });
    if (!changed.value)
        return CommandResult::invalidInput(changed.diagnostic);
    if (!*changed.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

std::optional<document::PrimitiveParameters> CadSession::selectedPrimitiveParameters() const {
    if (!selectedFeature_)
        return std::nullopt;
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, *selectedFeature_, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::primitive)
        return std::nullopt;
    return document_.primitiveParameters(*selectedFeature_);
}

} // namespace voelund::application
