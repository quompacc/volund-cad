#pragma once
#include <voelund/features/SketchIntersections.hpp>
namespace voelund::features {
struct RegionHalfEdge final {
    std::size_t interval;
    bool reversed;
};
struct SketchRegionBoundary final {
    std::vector<RegionHalfEdge> edges; // Counter-clockwise; holes reuse this in reverse.
    double area;
};
struct SketchRegion final {
    std::size_t outer;
    std::vector<std::size_t> holes; // Clockwise traversal of referenced boundaries.
    double area; // Outer area minus immediate holes; no implicit region selection.
};
enum class SketchRegionIssue {
    intersectionFailure,
    ambiguousConnection,
    tangentContact,
    openBoundary,
    degenerateBoundary,
    kernelFailure
};
struct SketchRegionDiagnostic final {
    SketchRegionIssue issue;
    std::vector<document::SketchElementId> elements;
};
struct SketchRegionResult final {
    SketchIntersectionResult split;
    std::vector<SketchRegionBoundary> boundaries;
    std::vector<SketchRegion> regions;
    std::vector<SketchRegionDiagnostic> diagnostics;
    [[nodiscard]] bool valid() const noexcept { return diagnostics.empty(); }
};
// Derived arrangement cells, no persistent IDs and no production face replacement.
// Any unresolved boundary invalidates the result; no consumable regions on failure.
[[nodiscard]] SketchRegionResult
buildSketchRegions(std::span<const document::SketchElement> elements);
} // namespace voelund::features
