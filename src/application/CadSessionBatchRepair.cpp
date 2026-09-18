#include <algorithm>
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
namespace {
const features::EvaluatedFeature* output(const features::FeatureGraphEvaluation& evaluation,
                                         document::FeatureId id) {
    const auto& values = evaluation.result.features;
    const auto found = std::ranges::find(values, id, &features::EvaluatedFeature::id);
    return found == values.end() ? nullptr : &*found;
}
document::SketchParameters repaired(const document::SketchParameters& original,
                                    const features::SketchSupportCandidate& chosen) {
    auto p = original;
    p.plane = chosen.plane;
    p.supportBinding = chosen.binding;
    const bool edges = std::ranges::any_of(p.constraints.values, [](const auto& c) {
        return (c.kind == document::SketchConstraintKind::coordinateX ||
                c.kind == document::SketchConstraintKind::coordinateY) &&
               c.auxiliaryValues.size() == 2;
    });
    p.repairReason =
        edges ? std::optional{document::SketchRepairReason::legacyEdges} : std::nullopt;
    return p;
}
} // namespace
std::vector<SketchSupportRepairSuggestion> CadSession::suggestSketchSupportRepairs() const {
    std::vector<SketchSupportRepairSuggestion> result;
    if (!evaluation_)
        return result;
    auto staged = document::DocumentSnapshot::capture(document_).toDocument(documentId());
    auto evaluated = evaluation_;
    const auto descriptors = document_.features();
    for (const auto index : evaluation_->dependencies.order()) {
        const auto id = evaluation_->dependencies.nodes()[index].feature;
        const auto feature = std::ranges::find(descriptors, id, &document::FeatureDescriptor::id);
        if (feature == descriptors.end() || feature->kind != document::FeatureKind::sketch ||
            !feature->inputFeature)
            continue;
        const auto original = document_.sketchParameters(id);
        auto review = features::reviewSketchReferences(documentId(), original,
                                                       output(*evaluated, *feature->inputFeature));
        if (review.status == features::SketchReferenceReviewStatus::current &&
            review.legacyEdgeConstraints.empty())
            continue;
        SketchSupportRepairSuggestion item{
            {documentId(), sceneStatus().document, id, *feature->inputFeature, std::move(review)},
            std::nullopt};
        // This prepares an explicit reviewable proposal, never publishes a migration.
        if (!original.supportBinding && item.inspection.review.legacyEdgeConstraints.empty() &&
            std::ranges::count_if(item.inspection.review.candidates,
                                  [](const auto& c) { return c.matchesOriginalPlane; }) == 1) {
            item.proposal =
                *std::ranges::find_if(item.inspection.review.candidates,
                                      [](const auto& c) { return c.matchesOriginalPlane; });
            (void)staged.setSketchParameters(id, repaired(original, *item.proposal));
            evaluated = features::FeatureGraph{}.evaluateDetailed(
                document::DocumentSnapshot::capture(staged), evaluated);
        }
        result.push_back(std::move(item));
    }
    return result;
}
CommandResult CadSession::repairSketchSupports(std::span<const SketchSupportRepair> repairs,
                                               ErrorDisposition disposition) {
    if (repairs.empty() || !evaluation_)
        return CommandResult::invalidInput("No repairs selected");
    std::vector<document::FeatureId> ids;
    for (const auto& request : repairs) {
        const auto& expected = request.expected;
        if (expected.document != documentId() || expected.stamp != sceneStatus().document ||
            std::ranges::find(ids, expected.feature) != ids.end())
            return CommandResult::invalidInput(
                "Repair list is stale or contains duplicate features. Refresh the list.");
        const auto descriptors = document_.features();
        const auto feature =
            std::ranges::find(descriptors, expected.feature, &document::FeatureDescriptor::id);
        if (feature == descriptors.end() || feature->kind != document::FeatureKind::sketch ||
            feature->inputFeature != expected.support ||
            document_.sketchParameters(expected.feature) != expected.review.original)
            return CommandResult::invalidInput(
                "An original repair reference changed. Refresh the list.");
        ids.push_back(expected.feature);
    }
    document::DocumentTransaction transaction(document_);
    auto evaluated = evaluation_;
    bool changed = false;
    for (const auto index : evaluation_->dependencies.order()) {
        const auto id = evaluation_->dependencies.nodes()[index].feature;
        const auto request =
            std::ranges::find_if(repairs, [&](const auto& r) { return r.expected.feature == id; });
        if (request == repairs.end())
            continue;
        const auto review =
            features::reviewSketchReferences(documentId(), request->expected.review.original,
                                             output(*evaluated, request->expected.support));
        const auto choice = std::ranges::find_if(review.candidates, [&](const auto& c) {
            return c.binding == request->chosen.binding && c.plane == request->chosen.plane;
        });
        if (choice == review.candidates.end())
            return CommandResult::invalidInput("A replacement is no longer available. Include its "
                                               "upstream repair or refresh the list.");
        changed = transaction.edit().setSketchParameters(id, repaired(review.original, *choice)) ||
                  changed;
        evaluated = featureGraph_.evaluateDetailed(
            document::DocumentSnapshot::capture(transaction.candidate()), evaluated);
    }
    if (!changed)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, repairs.back().expected.feature, CommandKind::editFeature,
                           disposition, true);
}
} // namespace voelund::application
