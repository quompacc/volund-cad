// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/application/Actions.hpp>

#include <string>

// M09: mirror a body ("Spiegeln N").
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createMirror(const CreateMirrorAction& action) {
    const auto body = resultBody(action.support);
    if (!body)
        return CommandResult::invalidInput("A mirror needs a current body");
    document::MirrorParameters parameters{
        *body,
        {action.basePlane, action.constructionPlane, std::nullopt},
        action.mode,
        action.features};
    if (action.faceSupport || action.face) {
        if (!action.faceSupport || !action.face)
            return CommandResult::invalidInput("A mirror face names its result and face");
        try {
            parameters.plane.face = faceReference(*action.faceSupport, *action.face);
        } catch (const std::invalid_argument& error) {
            return CommandResult::invalidInput(error.what());
        }
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::mirror) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addMirrorFeature("Spiegeln " + std::to_string(sequence), parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editMirrorParameters(document::FeatureId featureId,
                                               const document::MirrorParameters& parameters,
                                               ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setMirrorParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

// N12a: splits a body ("Teilen N"); with both sides kept, "Teilen N Teil 2" is the back.
CommandResult CadSession::createSplit(const CreateSplitAction& action) {
    const auto body = resultBody(action.support);
    if (!body)
        return CommandResult::invalidInput("A split needs a current body");
    document::SplitParameters parameters{
        *body, {action.basePlane, action.constructionPlane, std::nullopt}, action.keep};
    if (action.faceSupport || action.face) {
        if (!action.faceSupport || !action.face)
            return CommandResult::invalidInput("A split face names its result and face");
        try {
            parameters.plane.face = faceReference(*action.faceSupport, *action.face);
        } catch (const std::invalid_argument& error) {
            return CommandResult::invalidInput(error.what());
        }
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::split) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addSplitFeature("Teilen " + std::to_string(sequence), parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editSplitParameters(const EditSplitParametersAction& action) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setSplitParameters(action.split, action.parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature,
                           ErrorDisposition::reject, false);
}

document::MirrorParameters CadSession::mirrorParameters(document::FeatureId featureId) const {
    return document_.mirrorParameters(featureId);
}

} // namespace voelund::application
