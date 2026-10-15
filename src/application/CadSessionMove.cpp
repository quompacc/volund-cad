#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <string>

// M11 (user decision 2026-10-15): every body can be moved and turned. A primitive that is the
// last feature of its body keeps its own placement; a body whose last feature is a move changes
// that move; any other body gets a move ("Bewegen N") at the end of the history.
namespace voelund::application {
using namespace detail;

CommandResult CadSession::moveBody(document::FeatureId feature,
                                   const std::array<double, 3>& translation,
                                   std::optional<int> rotationAxis, double angle,
                                   ErrorDisposition disposition) {
    if (rotationAxis && (*rotationAxis < 0 || *rotationAxis > 2))
        return CommandResult::invalidInput("A body turns about X, Y or Z");
    const auto source = canTransformFeature(feature) ? transformBodyForFeature(feature)
                                                     : std::nullopt;
    if (!source)
        return CommandResult::invalidInput("Only a current body can be moved");
    const auto moved = [&](kernel::RigidTransform transform) {
        transform.translationX += translation[0];
        transform.translationY += translation[1];
        transform.translationZ += translation[2];
        kernel::Radians* rotations[] = {&transform.rotationX, &transform.rotationY,
                                        &transform.rotationZ};
        if (rotationAxis)
            *rotations[*rotationAxis] =
                kernel::Radians{rotations[*rotationAxis]->value() + angle};
        return transform;
    };
    if (const auto owner = placementFeature(*source))
        return editFeaturePlacement(*source, moved(document_.featurePlacement(*owner)),
                                    disposition);
    const auto descriptors = document_.features();
    const auto last = std::ranges::find(descriptors, *source, &document::FeatureDescriptor::id);
    if (last != descriptors.end() && last->kind == document::FeatureKind::move) {
        auto parameters = document_.moveParameters(*source);
        parameters.transform = moved(parameters.transform);
        return editMoveParameters(*source, parameters, disposition);
    }
    const auto body = resultBody(*source);
    const auto shape = featureShape(*source);
    if (!body || !shape)
        return CommandResult::invalidInput("Only a current body can be moved");
    // The pivot is the middle of the body now; later moves of this feature turn about it too.
    Bnd_Box bounds;
    BRepBndLib::Add(*shape, bounds);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    const document::MoveParameters parameters{
        *body,
        moved(kernel::RigidTransform{0, 0, 0, kernel::Radians{0}, kernel::Radians{0},
                                     kernel::Radians{0}}),
        {(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2}};
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::move) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addMoveFeature("Bewegen " + std::to_string(sequence), parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature, disposition,
                           true);
}

CommandResult CadSession::editMoveParameters(document::FeatureId featureId,
                                             const document::MoveParameters& parameters,
                                             ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setMoveParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::MoveParameters CadSession::moveParameters(document::FeatureId featureId) const {
    return document_.moveParameters(featureId);
}

} // namespace voelund::application
