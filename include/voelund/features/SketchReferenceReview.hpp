// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/features/FeatureGraph.hpp>

namespace voelund::features {
enum class SketchReferenceReviewStatus {
    current,
    legacyNeedsExplicitChoice,
    brokenBinding,
    invalidLegacyPlane,
    supportUnavailable
};
struct SketchSupportCandidate final {
    document::SketchSupportBinding binding;
    document::SketchPlane plane;
    // Geometric compatibility is a suggestion, never identity or migration authority.
    bool matchesOriginalPlane;
};
struct SketchReferenceReview final {
    SketchReferenceReviewStatus status;
    document::SketchParameters original;
    std::vector<SketchSupportCandidate> candidates;
    std::vector<document::SketchConstraintId> legacyEdgeConstraints;
    std::size_t unresolvedOrigins{0};
    std::string diagnostic;
};
// Read-only inspection of the original stored parameters and current support output.
// No nearest-face selection, index conversion, mutation or persistent candidate cache.
[[nodiscard]] SketchReferenceReview
reviewSketchReferences(document::DocumentId document, const document::SketchParameters& original,
                       const EvaluatedFeature* support);
} // namespace voelund::features
