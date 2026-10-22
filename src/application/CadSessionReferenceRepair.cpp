// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

namespace voelund::application {
namespace {
bool repairable(const document::ModelDocument& model, document::FeatureId id) {
    const auto descriptors = model.features();
    const auto found = std::ranges::find(descriptors, id, &document::FeatureDescriptor::id);
    return found != descriptors.end() && found->kind == document::FeatureKind::sketch &&
           found->inputFeature.has_value();
}
bool sameRequest(const SketchReferenceInspection& a, const SketchReferenceInspection& b) {
    return a.document == b.document && a.stamp == b.stamp && a.feature == b.feature &&
           a.support == b.support && a.review.original == b.review.original;
}
bool legacyEdge(const document::SketchConstraint& constraint) {
    return (constraint.kind == document::SketchConstraintKind::coordinateX ||
            constraint.kind == document::SketchConstraintKind::coordinateY) &&
           constraint.auxiliaryValues.size() == 2;
}
} // namespace
CommandResult CadSession::repairSketchSupport(const SketchReferenceInspection& expected,
                                              const features::SketchSupportCandidate& chosen,
                                              ErrorDisposition disposition) {
    if (expected.document != documentId() || expected.stamp != sceneStatus().document ||
        !repairable(document_, expected.feature))
        return CommandResult::invalidInput("Reference repair request is stale");
    const auto current = inspectSketchReferences(expected.feature);
    if (!sameRequest(expected, current))
        return CommandResult::invalidInput("Reference repair slot changed");
    const auto found = std::ranges::find_if(current.review.candidates, [&](const auto& candidate) {
        return candidate.binding == chosen.binding && candidate.plane == chosen.plane;
    });
    if (found == current.review.candidates.end())
        return CommandResult::invalidInput(
            "Support repair candidate is not current and uniquely named");
    auto parameters = current.review.original;
    parameters.supportBinding = found->binding;
    parameters.plane = found->plane;
    parameters.repairReason = std::ranges::any_of(parameters.constraints.values, legacyEdge)
                                  ? std::optional{document::SketchRepairReason::legacyEdges}
                                  : std::nullopt;
    document::DocumentTransaction transaction(document_);
    if (!transaction.edit().setSketchParameters(expected.feature, parameters))
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, expected.feature, CommandKind::editFeature, disposition,
                           true);
}
CommandResult CadSession::repairSketchEdgeReferences(
    const SketchReferenceInspection& expected, std::span<const document::SketchConstraintId> slots,
    const document::ProjectedEdgeReference& chosen, ErrorDisposition disposition) {
    if (expected.document != documentId() || expected.stamp != sceneStatus().document ||
        !repairable(document_, expected.feature) || slots.empty())
        return CommandResult::invalidInput("Edge repair request is stale or has no slots");
    const auto current = inspectSketchReferences(expected.feature);
    if (!sameRequest(expected, current) || !current.review.original.supportBinding || !evaluation_)
        return CommandResult::invalidInput("Repair support before repairing projected edges");
    const auto& outputs = evaluation_->result.features;
    const auto support =
        std::ranges::find(outputs, current.support, &features::EvaluatedFeature::id);
    if (support == outputs.end())
        return CommandResult::invalidInput("Current support unavailable");
    try {
        (void)features::resolveBoundEdgePoint(*current.review.original.supportBinding, *support,
                                              chosen);
    } catch (const std::invalid_argument& error) {
        return CommandResult::invalidInput(error.what());
    } catch (const features::GeometryEvaluationError& error) {
        return CommandResult::invalidInput(error.what());
    }
    auto parameters = current.review.original;
    std::vector<document::SketchConstraintId> visited;
    std::optional<document::SketchGeometryReference> point;
    std::optional<std::vector<double>> attachment;
    std::optional<std::optional<document::ProjectedEdgeReference>> previousEdge;
    for (const auto slot : slots) {
        auto found =
            std::ranges::find(parameters.constraints.values, slot, &document::SketchConstraint::id);
        if (found == parameters.constraints.values.end() ||
            (!legacyEdge(*found) && !found->projectedEdge) ||
            std::ranges::find(visited, slot) != visited.end() ||
            (point && *point != found->first) ||
            (attachment && *attachment != found->auxiliaryValues) ||
            (previousEdge && *previousEdge != found->projectedEdge))
            return CommandResult::invalidInput(
                "Edge repair slots must name one original legacy attachment");
        visited.push_back(slot);
        point = found->first;
        attachment = found->auxiliaryValues;
        previousEdge = found->projectedEdge;
        found->auxiliaryValues.clear();
        found->projectedEdge = chosen;
    }
    parameters.repairReason = std::ranges::any_of(parameters.constraints.values, legacyEdge)
                                  ? std::optional{document::SketchRepairReason::legacyEdges}
                                  : std::nullopt;
    document::DocumentTransaction transaction(document_);
    if (!transaction.edit().setSketchParameters(expected.feature, parameters))
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, expected.feature, CommandKind::editFeature, disposition,
                           true);
}
} // namespace voelund::application
