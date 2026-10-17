#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <string>

// M09: mirror a body ("Spiegeln N").
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createMirror(document::FeatureId support,
                                       std::optional<document::DatumPlane> basePlane,
                                       std::optional<document::FeatureId> constructionPlane,
                                       std::optional<document::FeatureId> faceSupport,
                                       std::optional<document::SubshapeOrigin> face,
                                       document::MirrorMode mode) {
    const auto body = resultBody(support);
    if (!body)
        return CommandResult::invalidInput("A mirror needs a current body");
    document::MirrorParameters parameters{
        *body, {basePlane, constructionPlane, std::nullopt}, mode};
    if (faceSupport || face) {
        if (!faceSupport || !face)
            return CommandResult::invalidInput("A mirror face names its result and face");
        try {
            parameters.plane.face = faceReference(*faceSupport, *face);
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

document::MirrorParameters CadSession::mirrorParameters(document::FeatureId featureId) const {
    return document_.mirrorParameters(featureId);
}

} // namespace voelund::application
