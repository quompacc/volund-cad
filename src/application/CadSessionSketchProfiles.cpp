#include "CommandInput.hpp"
#include <voelund/application/CadSession.hpp>

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace voelund::application {
namespace {

// The single unambiguous region whose outer boundary uses the element. Elements that bound
// several cells (crossing curves) select nothing until region picking by point exists.
std::optional<document::SketchRegionId>
regionByOuterElement(const features::SketchRegionIdentities& identities,
                     document::SketchElementId elementId) {
    std::optional<document::SketchRegionId> result;
    const auto& regions = identities.regions;
    for (std::size_t index = 0; index < regions.regions.size(); ++index) {
        const auto& boundary = regions.boundaries[regions.regions[index].outer];
        const bool uses = std::ranges::any_of(boundary.edges, [&](const auto& edge) {
            const auto piece = regions.split.intervals[edge.interval].piece;
            return regions.split.pieces[piece].element == elementId;
        });
        if (!uses)
            continue;
        if (result || identities.ambiguous(index))
            return std::nullopt;
        result = identities.ids[index];
    }
    return result;
}

bool isSketch(const document::ModelDocument& document, document::FeatureId feature) {
    const auto descriptors = document.features();
    const auto descriptor =
        std::ranges::find(descriptors, feature, &document::FeatureDescriptor::id);
    return descriptor != descriptors.end() && descriptor->kind == document::FeatureKind::sketch;
}

} // namespace

bool ExtrusionSelectionState::needsRepair() const noexcept {
    return !legacyProfiles.empty() ||
           std::ranges::any_of(regions, [](const ExtrusionRegionState& state) {
               return state.status != features::SketchRegionReferenceStatus::resolved;
           });
}

std::span<const document::SketchRegionId> CadSession::selectedSketchProfiles() const noexcept {
    return selectedSketchProfiles_;
}

bool CadSession::selectSketchProfileByElement(document::SketchElementId elementId, bool additive) {
    if (!selectedFeature_.has_value() || !isSketch(document_, *selectedFeature_))
        return false;
    const auto& elements = document_.sketchParameters(*selectedFeature_).elements.values;
    const auto region = regionByOuterElement(features::identifySketchRegions(elements), elementId);
    if (!region)
        return false;

    if (!additive)
        selectedSketchProfiles_.clear();
    const auto selected = std::ranges::find(selectedSketchProfiles_, *region);
    if (additive && selected != selectedSketchProfiles_.end())
        selectedSketchProfiles_.erase(selected);
    else if (selected == selectedSketchProfiles_.end())
        selectedSketchProfiles_.push_back(*region);
    selectedSketchElement_ = elementId;
    return true;
}

std::vector<document::SketchElementId> CadSession::selectedSketchProfileElements() const {
    std::vector<document::SketchElementId> result;
    if (!selectedFeature_.has_value() || selectedSketchProfiles_.empty())
        return result;
    for (const document::SketchRegionId& region : selectedSketchProfiles_)
        for (document::SketchElementId element : region.elements())
            if (std::ranges::find(result, element) == result.end())
                result.push_back(element);
    return result;
}

bool CadSession::canExtrudeSelectedSketchProfiles() const noexcept {
    if (!selectedFeature_ || !featureShape(*selectedFeature_))
        return false;
    if (!selectedFeature_.has_value() || selectedSketchProfiles_.empty())
        return false;
    return isSketch(document_, *selectedFeature_);
}

ExtrusionSelectionState CadSession::inspectExtrusionSelection(document::FeatureId extrusion) const {
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, extrusion, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::extrusion ||
        !descriptor->inputFeature)
        throw std::invalid_argument("Selection inspection requires an extrusion");
    const auto& parameters = document_.extrusionParameters(extrusion);
    ExtrusionSelectionState state{parameters.profiles, {}};
    if (parameters.regions.empty())
        return state;
    const auto& elements = document_.sketchParameters(*descriptor->inputFeature).elements.values;
    const auto identities = features::identifySketchRegions(elements);
    for (const document::SketchRegionId& region : parameters.regions)
        state.regions.push_back(
            {region, features::resolveSketchRegion(identities, elements, region).status});
    return state;
}

CommandResult
CadSession::createSketchProfileExtrusion(document::FeatureId sketchFeature,
                                         std::span<const document::SketchRegionId> profileIds,
                                         double distance) {
    if (!std::isfinite(distance) || distance <= 0.0 || profileIds.empty())
        return CommandResult::invalidInput(
            "Profile extrusion requires selected profiles and a positive finite distance");
    if (!isSketch(document_, sketchFeature))
        return CommandResult::invalidInput("Profile extrusion requires an existing sketch");
    const auto& elements = document_.sketchParameters(sketchFeature).elements.values;
    const auto identities = features::identifySketchRegions(elements);
    for (const document::SketchRegionId& region : profileIds) {
        if (features::resolveSketchRegion(identities, elements, region).status !=
            features::SketchRegionReferenceStatus::resolved)
            return CommandResult::invalidInput("Profile extrusion references an unknown profile");
    }
    std::size_t sequence = 1;
    for (const document::FeatureDescriptor& feature : document_.features()) {
        if (feature.kind == document::FeatureKind::extrusion)
            ++sequence;
    }
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::vector<document::SketchRegionId> selected(profileIds.begin(), profileIds.end());
    const auto input = detail::prepareInput([&] {
        return candidate.addExtrusionFeature("Extrusion " + std::to_string(sequence), sketchFeature,
                                             document::ExtrusionParameters{distance, {}, selected});
    });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    return finishCandidate(transaction, *input.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult
CadSession::createDatumSketchExtrusion(const document::SketchParameters& sketch,
                                       std::span<const document::SketchRegionId> regions,
                                       double distance) {
    const auto count = [&](document::FeatureKind kind) {
        return std::ranges::count(document_.features(), kind, &document::FeatureDescriptor::kind);
    };
    const auto sketchName = "Skizze " + std::to_string(count(document::FeatureKind::sketch) + 1);
    const auto extrusionName =
        "Extrusion " + std::to_string(count(document::FeatureKind::extrusion) + 1);
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto sketchId =
        detail::prepareInput([&] { return candidate.addDatumSketchFeature(sketchName, sketch); });
    if (!sketchId.value)
        return CommandResult::invalidInput(sketchId.diagnostic);
    // Step 2 reads the elements the candidate stored; any failure discards step 1 as well.
    const auto elements = candidate.sketchParameters(*sketchId.value).elements.values;
    const auto identities = features::identifySketchRegions(elements);
    std::vector<document::SketchRegionId> selected(regions.begin(), regions.end());
    if (selected.empty())
        for (std::size_t index = 0; index < identities.ids.size(); ++index)
            if (!identities.ambiguous(index))
                selected.push_back(identities.ids[index]);
    if (selected.empty())
        return CommandResult::invalidInput("The sketch has no closed region to extrude");
    for (const document::SketchRegionId& region : selected)
        if (features::resolveSketchRegion(identities, elements, region).status !=
            features::SketchRegionReferenceStatus::resolved)
            return CommandResult::invalidInput("Profile extrusion references an unknown profile");
    const auto extrusionId = detail::prepareInput([&] {
        return candidate.addExtrusionFeature(extrusionName, *sketchId.value,
                                             document::ExtrusionParameters{distance, {}, selected});
    });
    if (!extrusionId.value)
        return CommandResult::invalidInput(extrusionId.diagnostic);
    return finishCandidate(transaction, *extrusionId.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

} // namespace voelund::application
