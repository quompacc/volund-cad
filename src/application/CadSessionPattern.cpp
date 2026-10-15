#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <string>

// M10: linear and circular patterns of a body ("Muster N").
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createPattern(document::FeatureId support, const PatternInput& input) {
    const auto body = resultBody(support);
    if (!body)
        return CommandResult::invalidInput("A pattern needs a current body");
    document::PatternParameters parameters{*body,
                                           input.kind,
                                           {},
                                           input.datumAxis,
                                           input.reversed,
                                           input.count,
                                           input.spacing,
                                           kernel::Radians{input.angle},
                                           input.mode};
    parameters.axis.baseAxis = input.baseAxis;
    if (input.edgeSupport || input.edge) {
        const auto edgeBody = input.edgeSupport ? resultBody(*input.edgeSupport) : std::nullopt;
        if (!edgeBody || !input.edge)
            return CommandResult::invalidInput("A pattern edge names a current body edge");
        parameters.axis.body = edgeBody;
        parameters.axis.edge = *input.edge;
        parameters.axis.edge->id = 1;
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::pattern) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addPatternFeature("Muster " + std::to_string(sequence), parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editPatternParameters(document::FeatureId featureId,
                                                const document::PatternParameters& parameters,
                                                ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setPatternParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::PatternParameters CadSession::patternParameters(document::FeatureId featureId) const {
    return document_.patternParameters(featureId);
}

} // namespace voelund::application
