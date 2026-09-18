#include "CommandInput.hpp"
#include <voelund/application/CadSession.hpp>

#include <voelund/features/SketchProfiles.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace voelund::application {
namespace {

const features::SketchProfile* profileContaining(std::span<const features::SketchProfile> profiles,
                                                 document::SketchElementId elementId) {
    for (const features::SketchProfile& profile : profiles) {
        if (std::ranges::find(profile.elementIds, elementId) != profile.elementIds.end())
            return &profile;
    }
    return nullptr;
}

bool containsProfile(std::span<const features::SketchProfile> profiles,
                     document::SketchProfileId profileId) {
    return std::ranges::find(profiles, profileId, &features::SketchProfile::id) != profiles.end();
}

} // namespace

std::span<const document::SketchProfileId> CadSession::selectedSketchProfiles() const noexcept {
    return selectedSketchProfiles_;
}

bool CadSession::selectSketchProfileByElement(document::SketchElementId elementId, bool additive) {
    if (!selectedFeature_.has_value())
        return false;
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, *selectedFeature_, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::sketch)
        return false;
    const auto profiles = features::detectClosedProfiles(
        document_.sketchParameters(*selectedFeature_).elements.values);
    const features::SketchProfile* profile = profileContaining(profiles, elementId);
    if (profile == nullptr)
        return false;

    if (!additive)
        selectedSketchProfiles_.clear();
    const auto selected = std::ranges::find(selectedSketchProfiles_, profile->id);
    if (additive && selected != selectedSketchProfiles_.end())
        selectedSketchProfiles_.erase(selected);
    else if (selected == selectedSketchProfiles_.end())
        selectedSketchProfiles_.push_back(profile->id);
    selectedSketchElement_ = elementId;
    return true;
}

std::vector<document::SketchElementId> CadSession::selectedSketchProfileElements() const {
    std::vector<document::SketchElementId> result;
    if (!selectedFeature_.has_value() || selectedSketchProfiles_.empty())
        return result;
    const auto profiles = features::detectClosedProfiles(
        document_.sketchParameters(*selectedFeature_).elements.values);
    for (const features::SketchProfile& profile : profiles) {
        if (std::ranges::find(selectedSketchProfiles_, profile.id) != selectedSketchProfiles_.end())
            result.insert(result.end(), profile.elementIds.begin(), profile.elementIds.end());
    }
    return result;
}

bool CadSession::canExtrudeSelectedSketchProfiles() const noexcept {
    if (!selectedFeature_ || !featureShape(*selectedFeature_))
        return false;
    if (!selectedFeature_.has_value() || selectedSketchProfiles_.empty())
        return false;
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, *selectedFeature_, &document::FeatureDescriptor::id);
    return descriptor != descriptors.end() && descriptor->kind == document::FeatureKind::sketch;
}

CommandResult
CadSession::createSketchProfileExtrusion(document::FeatureId sketchFeature,
                                         std::span<const document::SketchProfileId> profileIds,
                                         double distance) {
    if (!std::isfinite(distance) || distance <= 0.0 || profileIds.empty())
        return CommandResult::invalidInput(
            "Profile extrusion requires selected profiles and a positive finite distance");
    const auto descriptor =
        std::ranges::find(document_.features(), sketchFeature, &document::FeatureDescriptor::id);
    if (descriptor == document_.features().end() ||
        descriptor->kind != document::FeatureKind::sketch)
        return CommandResult::invalidInput("Profile extrusion requires an existing sketch");
    const auto profiles =
        features::detectClosedProfiles(document_.sketchParameters(sketchFeature).elements.values);
    for (document::SketchProfileId profileId : profileIds) {
        if (!containsProfile(profiles, profileId))
            return CommandResult::invalidInput("Profile extrusion references an unknown profile");
    }
    std::size_t sequence = 1;
    for (const document::FeatureDescriptor& feature : document_.features()) {
        if (feature.kind == document::FeatureKind::extrusion)
            ++sequence;
    }
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::vector<document::SketchProfileId> selected(profileIds.begin(), profileIds.end());
    const auto input = detail::prepareInput([&] {
        return candidate.addExtrusionFeature("Extrusion " + std::to_string(sequence), sketchFeature,
                                             document::ExtrusionParameters{distance, selected});
    });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    return finishCandidate(transaction, *input.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

} // namespace voelund::application
