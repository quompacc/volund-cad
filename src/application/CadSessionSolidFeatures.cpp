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

} // namespace voelund::application
