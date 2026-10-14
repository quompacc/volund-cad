#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <algorithm>
#include <string>

// Commands of the features added on 2026-10-14 (M19-M27).
namespace voelund::application {
using namespace detail;

std::optional<document::BodyId> CadSession::resultBody(document::FeatureId feature) const {
    if (!canAddCylinder(feature))
        return std::nullopt;
    const auto& outputs = evaluation_->result.features;
    const auto output = std::ranges::find(outputs, feature, &features::EvaluatedFeature::id);
    return output == outputs.end() ? std::nullopt : output->bodyId;
}

CommandResult CadSession::createCombine(document::FeatureId target,
                                        std::span<const document::FeatureId> tools,
                                        document::CombineOperation operation, bool keepTools) {
    const auto targetBody = resultBody(target);
    if (!targetBody)
        return CommandResult::invalidInput("The target is no current body");
    document::CombineParameters parameters{*targetBody, {}, operation, keepTools};
    for (const auto tool : tools) {
        const auto body = resultBody(tool);
        if (!body)
            return CommandResult::invalidInput("A tool is no current body");
        parameters.tools.push_back(*body);
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::combine) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addCombineFeature("Kombinieren " + std::to_string(sequence), parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editCombineParameters(document::FeatureId featureId,
                                                const document::CombineParameters& parameters,
                                                ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setCombineParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::CombineParameters CadSession::combineParameters(document::FeatureId featureId) const {
    return document_.combineParameters(featureId);
}

std::optional<document::SketchPlane> CadSession::datumFrame(document::FeatureId feature) const {
    if (!evaluation_)
        return std::nullopt;
    const auto& outputs = evaluation_->result.features;
    const auto output = std::ranges::find(outputs, feature, &features::EvaluatedFeature::id);
    return output == outputs.end() ? std::nullopt : output->datumFrame;
}

CommandResult CadSession::createDatumPlane(std::optional<document::DatumPlane> basePlane,
                                           std::optional<document::FeatureId> support,
                                           std::optional<document::SubshapeOrigin> face,
                                           double offset, document::DatumRotationAxis rotationAxis,
                                           double angle) {
    document::DatumPlaneParameters parameters;
    parameters.basePlane = basePlane;
    parameters.offset = offset;
    parameters.rotationAxis = rotationAxis;
    if (support || face) {
        if (!support || !face)
            return CommandResult::invalidInput("A face reference names its result and face");
        try {
            parameters.face = faceReference(*support, *face);
        } catch (const std::invalid_argument& error) {
            return CommandResult::invalidInput(error.what());
        }
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::datumPlane) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        parameters.angle = kernel::Radians{angle};
        return candidate.addDatumPlaneFeature("Bezugsebene " + std::to_string(sequence),
                                              parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editDatumPlaneParameters(document::FeatureId featureId,
                                                   const document::DatumPlaneParameters& parameters,
                                                   ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setDatumPlaneParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::createDatumAxis(std::optional<document::BaseAxis> baseAxis,
                                          std::optional<document::FeatureId> support,
                                          std::optional<document::FilletEdge> edge) {
    document::DatumAxisParameters parameters;
    parameters.baseAxis = baseAxis;
    if (support || edge) {
        const auto body = support ? resultBody(*support) : std::nullopt;
        if (!body || !edge)
            return CommandResult::invalidInput("An edge reference names a current body edge");
        parameters.body = body;
        parameters.edge = *edge;
        parameters.edge->id = 1;
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::datumAxis) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addDatumAxisFeature("Bezugsachse " + std::to_string(sequence),
                                             parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editDatumAxisParameters(document::FeatureId featureId,
                                                  const document::DatumAxisParameters& parameters,
                                                  ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setDatumAxisParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::createShell(document::FeatureId support,
                                     std::span<const document::SubshapeOrigin> faces,
                                     double thickness, bool outward) {
    const auto body = resultBody(support);
    if (!body)
        return CommandResult::invalidInput("A shell needs a current body");
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::shell) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addShellFeature(
            "Wandstärke " + std::to_string(sequence),
            document::ShellParameters{*body, {faces.begin(), faces.end()}, thickness, outward});
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editShellParameters(document::FeatureId featureId,
                                              const document::ShellParameters& parameters,
                                              ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setShellParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::ShellParameters CadSession::shellParameters(document::FeatureId featureId) const {
    return document_.shellParameters(featureId);
}

} // namespace voelund::application
