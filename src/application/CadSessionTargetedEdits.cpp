// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CommandInput.hpp"
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
// AI02: edits with explicit targets. The selection decides nothing here and is kept.
CommandResult CadSession::editBoxDimensions(document::FeatureId featureId,
                                            const kernel::BoxDimensions& dimensions,
                                            ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setBoxDimensions(featureId, dimensions); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::editBoxParameters(document::FeatureId featureId,
                                            const document::BoxParameters& parameters,
                                            ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setBoxParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::editFeaturePlacement(document::FeatureId featureId,
                                               const kernel::RigidTransform& placement,
                                               ErrorDisposition disposition) {
    const std::optional<document::FeatureId> owner = placementFeature(featureId);
    if (!owner.has_value())
        return CommandResult::invalidInput("Feature result has no rigid placement");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setFeaturePlacement(*owner, placement); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}
} // namespace voelund::application
