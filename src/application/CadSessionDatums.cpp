// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/application/Actions.hpp>

#include <string>

// N13b: construction planes of every kind (on a base plane or face, a midplane between two
// faces, through three points) and construction points.
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createDatumPlane(const CreateDatumPlaneAction& action) {
    document::DatumPlaneParameters parameters;
    parameters.basePlane = action.basePlane;
    parameters.offset = action.offset.value;
    parameters.rotationAxis = action.rotationAxis;
    parameters.angle = kernel::Radians{action.angle.value};
    parameters.points = action.points;
    try {
        if (action.support && action.face)
            parameters.face = faceReference(*action.support, *action.face);
        if (action.secondSupport && action.secondFace)
            parameters.secondFace = faceReference(*action.secondSupport, *action.secondFace);
    } catch (const std::invalid_argument& error) {
        return CommandResult::invalidInput(error.what());
    }
    if (action.support.has_value() != action.face.has_value() ||
        action.secondSupport.has_value() != action.secondFace.has_value())
        return CommandResult::invalidInput("A face reference names its result and face");
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::datumPlane) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addDatumPlaneFeature("Bezugsebene " + std::to_string(sequence),
                                              parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::createDatumPoint(const CreateDatumPointAction& action) {
    document::DatumPointParameters parameters{action.position, {}, action.edge, action.spot};
    if (action.edge) {
        const auto body = action.support ? resultBody(*action.support) : std::nullopt;
        if (!body)
            return CommandResult::invalidInput("A point on an edge needs a current body");
        parameters.body = body;
        parameters.edge->id = 1;
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::datumPoint) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addDatumPointFeature("Bezugspunkt " + std::to_string(sequence),
                                              parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editDatumPoint(const EditDatumPointParametersAction& action) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setDatumPointParameters(action.point, action.parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature,
                           ErrorDisposition::reject, false);
}

} // namespace voelund::application
