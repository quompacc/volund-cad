#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <algorithm>
#include <string>

// N02b (user decision 2026-10-21): several bodies or timeline steps chosen together are moved
// or suppressed as one undo step. A group is only moved, not turned: the bodies keep their own
// pivots, so one turn would mean different things for primitives and for moved bodies.
namespace voelund::application {
using namespace detail;

namespace {

kernel::RigidTransform shifted(kernel::RigidTransform transform,
                               const std::array<double, 3>& translation) {
    transform.translationX += translation[0];
    transform.translationY += translation[1];
    transform.translationZ += translation[2];
    return transform;
}

} // namespace

bool CadSession::addGroupMove(document::ModelDocument& candidate, document::FeatureId source,
                              const std::array<double, 3>& translation, std::size_t& sequence) {
    if (const auto owner = placementFeature(source))
        return candidate.setFeaturePlacement(
            *owner, shifted(document_.featurePlacement(*owner), translation));
    const auto descriptors = document_.features();
    const auto last = std::ranges::find(descriptors, source, &document::FeatureDescriptor::id);
    if (last != descriptors.end() && last->kind == document::FeatureKind::move) {
        auto parameters = document_.moveParameters(source);
        parameters.transform = shifted(parameters.transform, translation);
        return candidate.setMoveParameters(source, parameters);
    }
    const auto body = resultBody(source);
    const auto shape = featureShape(source);
    if (!body || !shape)
        throw std::invalid_argument("Only current bodies can be moved");
    Bnd_Box bounds;
    BRepBndLib::Add(*shape, bounds);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    const document::MoveParameters parameters{
        *body,
        shifted({0, 0, 0, kernel::Radians{0}, kernel::Radians{0}, kernel::Radians{0}}, translation),
        {(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2}};
    static_cast<void>(
        candidate.addMoveFeature("Bewegen " + std::to_string(sequence++), parameters));
    return true;
}

CommandResult CadSession::moveBodies(std::span<const document::FeatureId> features,
                                     const std::array<double, 3>& translation,
                                     ErrorDisposition disposition) {
    std::vector<document::FeatureId> sources;
    for (const auto feature : features) {
        const auto source =
            canTransformFeature(feature) ? transformBodyForFeature(feature) : std::nullopt;
        if (!source)
            return CommandResult::invalidInput("Only current bodies can be moved");
        if (std::ranges::find(sources, *source) == sources.end())
            sources.push_back(*source);
    }
    if (sources.empty())
        return CommandResult::invalidInput("No body to move");
    std::size_t sequence = featureCount(document_.features(), document::FeatureKind::move) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput([&] {
        bool changed = false;
        for (const auto source : sources)
            changed = addGroupMove(candidate, source, translation, sequence) || changed;
        return changed;
    });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::setFeaturesSuppressed(std::span<const document::FeatureId> features,
                                                bool suppressed, ErrorDisposition disposition) {
    if (features.empty())
        return CommandResult::invalidInput("No step chosen");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput([&] {
        bool changed = false;
        for (const auto feature : features)
            changed = candidate.setFeatureSuppressed(feature, suppressed) || changed;
        return changed;
    });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

} // namespace voelund::application
