#pragma once
#include <voelund/document/SketchRegionId.hpp>
#include <voelund/features/SketchRegions.hpp>
namespace voelund::features {
// R09: persistent identities for the derived R08b regions.
struct SketchRegionIdentities final {
    SketchRegionResult regions;
    std::vector<document::SketchRegionId> ids; // Parallel to regions.regions; empty if invalid.
    // True when another cell carries the same signature; such a cell is never resolved.
    [[nodiscard]] bool ambiguous(std::size_t region) const;
};
enum class SketchRegionReferenceStatus {
    resolved,
    elementsRemoved, // A referenced element is gone or became construction geometry.
    topologyChanged, // All elements remain, but a split/merge/new cut changed the boundary.
    ambiguous,       // Several cells share the signature; never guessed.
    invalidSketch    // Region construction failed; see regions.diagnostics.
};
struct SketchRegionResolution final {
    SketchRegionReferenceStatus status;
    std::optional<std::size_t> region; // Index into regions.regions when resolved.
};
[[nodiscard]] SketchRegionIdentities
identifySketchRegions(std::span<const document::SketchElement> elements);
// Exact, unique signature match only; anything else requires explicit repair.
[[nodiscard]] SketchRegionResolution
resolveSketchRegion(const SketchRegionIdentities& identities,
                    std::span<const document::SketchElement> elements,
                    const document::SketchRegionId& id);
// Documented migration of a legacy minimum-element profile ID: the region whose outer
// boundary carries exactly the elements of that loop plus every region nested inside it,
// i.e. the filled area the legacy extrusion produced. std::nullopt if not unique.
[[nodiscard]] std::optional<std::vector<document::SketchRegionId>>
legacyProfileRegions(const SketchRegionIdentities& identities,
                     std::span<const document::SketchElement> elements,
                     document::SketchProfileId profile);
} // namespace voelund::features
