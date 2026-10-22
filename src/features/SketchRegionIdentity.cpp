// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <set>
#include <voelund/features/SketchProfiles.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
namespace voelund::features {
namespace {
std::vector<document::SketchRegionBoundaryUse> boundaryUses(const SketchRegionResult& r,
                                                            std::size_t boundary) {
    std::vector<document::SketchRegionBoundaryUse> uses;
    for (const RegionHalfEdge& edge : r.boundaries[boundary].edges) {
        const ContourPiece& piece = r.split.pieces[r.split.intervals[edge.interval].piece];
        const bool curved = !std::holds_alternative<document::SketchLine>(piece.geometry) &&
                            !std::holds_alternative<document::SketchRectangle>(piece.geometry);
        uses.push_back({piece.element, piece.part,
                        curved ? std::optional<bool>{edge.reversed} : std::nullopt});
    }
    return uses;
}
std::set<std::uint64_t> outerElements(const SketchRegionResult& r, std::size_t region) {
    std::set<std::uint64_t> elements;
    for (const auto& use : boundaryUses(r, r.regions[region].outer))
        elements.insert(use.element.value());
    return elements;
}
std::optional<std::size_t> regionWithOuter(const SketchRegionResult& r, std::size_t boundary) {
    const auto found = std::ranges::find(r.regions, boundary, &SketchRegion::outer);
    if (found == r.regions.end())
        return std::nullopt;
    return static_cast<std::size_t>(found - r.regions.begin());
}
} // namespace

bool SketchRegionIdentities::ambiguous(std::size_t region) const {
    return std::ranges::count(ids, ids.at(region)) > 1;
}

SketchRegionIdentities identifySketchRegions(std::span<const document::SketchElement> elements) {
    SketchRegionIdentities result{buildSketchRegions(elements), {}};
    if (!result.regions.valid())
        return result;
    for (const SketchRegion& region : result.regions.regions) {
        std::vector<std::vector<document::SketchRegionBoundaryUse>> holes;
        for (std::size_t hole : region.holes)
            holes.push_back(boundaryUses(result.regions, hole));
        result.ids.push_back(document::SketchRegionId::fromBoundaries(
            boundaryUses(result.regions, region.outer), std::move(holes)));
    }
    return result;
}

SketchRegionResolution resolveSketchRegion(const SketchRegionIdentities& identities,
                                           std::span<const document::SketchElement> elements,
                                           const document::SketchRegionId& id) {
    std::set<std::uint64_t> material;
    for (const document::SketchElement& element : elements)
        if (!element.construction)
            material.insert(element.id.value());
    for (document::SketchElementId element : id.elements())
        if (!material.contains(element.value()))
            return {SketchRegionReferenceStatus::elementsRemoved, std::nullopt};
    if (!identities.regions.valid())
        return {SketchRegionReferenceStatus::invalidSketch, std::nullopt};
    const auto found = std::ranges::find(identities.ids, id);
    if (found == identities.ids.end())
        return {SketchRegionReferenceStatus::topologyChanged, std::nullopt};
    const auto index = static_cast<std::size_t>(found - identities.ids.begin());
    if (identities.ambiguous(index))
        return {SketchRegionReferenceStatus::ambiguous, std::nullopt};
    return {SketchRegionReferenceStatus::resolved, index};
}

std::optional<std::vector<document::SketchRegionId>>
legacyProfileRegions(const SketchRegionIdentities& identities,
                     std::span<const document::SketchElement> elements,
                     document::SketchProfileId profile) {
    if (!identities.regions.valid())
        return std::nullopt;
    const auto profiles = detectClosedProfiles(elements);
    const auto loop = std::ranges::find(profiles, profile, &SketchProfile::id);
    if (loop == profiles.end())
        return std::nullopt;
    std::set<std::uint64_t> loopElements;
    for (document::SketchElementId element : loop->elementIds)
        loopElements.insert(element.value());
    std::optional<std::size_t> start;
    for (std::size_t region = 0; region < identities.regions.regions.size(); ++region) {
        if (outerElements(identities.regions, region) != loopElements)
            continue;
        if (start)
            return std::nullopt; // Several cells claim the loop; no guessing.
        start = region;
    }
    if (!start)
        return std::nullopt;
    std::vector<document::SketchRegionId> result;
    std::vector<std::size_t> pending{*start};
    while (!pending.empty()) {
        const std::size_t region = pending.back();
        pending.pop_back();
        if (identities.ambiguous(region))
            return std::nullopt;
        result.push_back(identities.ids[region]);
        for (std::size_t hole : identities.regions.regions[region].holes) {
            const auto inner = regionWithOuter(identities.regions, hole);
            if (!inner)
                return std::nullopt;
            pending.push_back(*inner);
        }
    }
    std::ranges::sort(result);
    return result;
}
} // namespace voelund::features
