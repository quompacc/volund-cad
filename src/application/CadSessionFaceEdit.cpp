// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/application/Actions.hpp>

#include <string>

// N13a: faces of a body moved or removed ("Flächen N").
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createFaceEdit(const CreateFaceEditAction& action) {
    const auto body = resultBody(action.support);
    if (!body)
        return CommandResult::invalidInput("A face edit needs a current body");
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::faceEdit) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addFaceEditFeature(
            "Flächen " + std::to_string(sequence),
            document::FaceEditParameters{*body, action.faces, action.mode, action.distance});
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editFaceEdit(const EditFaceEditParametersAction& action) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setFaceEditParameters(action.edit, action.parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature,
                           ErrorDisposition::reject, false);
}

} // namespace voelund::application
