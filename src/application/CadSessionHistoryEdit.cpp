#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <set>

// M16 (user decision 2026-10-16): reorder and delete features.
namespace voelund::application {
using namespace detail;
namespace {

// Features that evaluate in `before` but not in `after` (by ID).
std::vector<document::FeatureId> newlyFailing(const features::EvaluationReport& before,
                                              const features::EvaluationReport& after) {
    std::vector<document::FeatureId> failing;
    for (const auto& issue : after.features) {
        if (issue.status != features::EvaluationStatus::failed &&
            issue.status != features::EvaluationStatus::blockedByDependency)
            continue;
        const auto earlier = std::ranges::find(before.features, issue.feature,
                                               &features::EvaluationIssue::feature);
        if (earlier != before.features.end() &&
            (earlier->status == features::EvaluationStatus::valid))
            failing.push_back(issue.feature);
    }
    return failing;
}

} // namespace

CommandResult CadSession::moveFeature(document::FeatureId featureId,
                                      std::optional<document::FeatureId> after) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.moveFeature(featureId, after); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, featureId, CommandKind::editFeature,
                           ErrorDisposition::reject, false);
}

DeletionPlan CadSession::deletionPlan(document::FeatureId featureId) const {
    DeletionPlan plan;
    const auto snapshot = document::DocumentSnapshot::capture(document_);
    const auto features = snapshot.features();
    // Structurally needed: what cannot exist without the deleted features (its sketch's
    // extrusions, the features of a body it creates, ...), transitively.
    std::set<std::uint64_t> removed{featureId.value()};
    for (std::size_t index = 0; index < features.size(); ++index)
        if (document::companionOwner(snapshot.parameters()[index]) == featureId)
            removed.insert(features[index].id.value());
    const auto creates = document::featureBodyContexts(features, snapshot.parameters());
    for (bool grew = true; grew;) {
        grew = false;
        for (std::size_t index = 0; index < features.size(); ++index) {
            const auto& feature = features[index];
            if (removed.contains(feature.id.value()))
                continue;
            const auto references = document::featureReferences(feature, snapshot.parameters()[index]);
            const bool needs = std::ranges::any_of(references.structural, [&](document::FeatureId id) {
                if (!removed.contains(id.value()))
                    return false;
                // A body input is taken over by the body's earlier state, unless the body
                // itself is gone with its creator.
                if (feature.inputFeature != id)
                    return true;
                const auto input = std::ranges::find(features, id, &document::FeatureDescriptor::id);
                const auto body = creates[static_cast<std::size_t>(input - features.begin())];
                return input->kind == document::FeatureKind::sketch || !body ||
                       removed.contains(body->value());
            });
            if (needs) {
                removed.insert(feature.id.value());
                grew = true;
            }
        }
    }
    plan.onlyThisPossible =
        std::ranges::all_of(removed, [&](std::uint64_t id) {
            if (id == featureId.value())
                return true;
            const auto found = std::ranges::find(features, document::FeatureId{id},
                                                 &document::FeatureDescriptor::id);
            return document::companionOwner(snapshot.parameters()[static_cast<std::size_t>(
                       found - features.begin())]) == featureId;
        });
    // Then what would fail without them, repeated until nothing new fails.
    const auto before = evaluation_ ? evaluation_->report : features::EvaluationReport{};
    for (int round = 0; round < 32; ++round) {
        auto candidate = document_;
        std::vector<document::FeatureId> ids;
        for (const auto id : removed)
            ids.emplace_back(id);
        try {
            static_cast<void>(candidate.deleteFeatures(ids));
        } catch (const std::exception&) {
            break;
        }
        const auto evaluated =
            featureGraph_.evaluateDetailed(document::DocumentSnapshot::capture(candidate));
        bool grew = false;
        for (const auto failing : newlyFailing(before, evaluated->report))
            grew = removed.insert(failing.value()).second || grew;
        if (!grew)
            break;
    }
    for (const auto& feature : features)
        if (removed.contains(feature.id.value()) && feature.id != featureId &&
            !document::isCompanion(feature.kind))
            plan.dependents.push_back(feature.id);
    // The cascade removes them all; without them only the feature (with its copies).
    for (const auto& feature : features)
        if (removed.contains(feature.id.value()))
            plan.cascade.push_back(feature.id);
    return plan;
}

CommandResult CadSession::deleteFeatures(std::span<const document::FeatureId> featureIds,
                                         ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.deleteFeatures(featureIds); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, std::nullopt, CommandKind::editFeature, disposition,
                           false);
}

} // namespace voelund::application
