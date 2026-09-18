#pragma once
#include <voelund/application/SceneStatus.hpp>
#include <voelund/features/SketchReferenceReview.hpp>
namespace voelund::application {
struct SketchReferenceInspection final {
    document::DocumentId document;
    DocumentStamp stamp;
    document::FeatureId feature;
    document::FeatureId support;
    features::SketchReferenceReview review;
};
struct SketchSupportRepair final {
    SketchReferenceInspection expected;
    features::SketchSupportCandidate chosen;
};
struct SketchSupportRepairSuggestion final {
    SketchReferenceInspection inspection;
    std::optional<features::SketchSupportCandidate> proposal;
};
} // namespace voelund::application
