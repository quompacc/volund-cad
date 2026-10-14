#pragma once
#include <voelund/kernel/Prism.hpp>
#include <voelund/kernel/Revolution.hpp>
#include <voelund/features/SketchEvaluation.hpp>

namespace voelund::features {
[[nodiscard]] std::vector<EdgeProvenance>
sketchEdgeOrigins(document::FeatureId sketch, std::span<const EvaluatedSketchElement> elements);
// Only the builder's explicit first/last contours are named. Generated rails and
// Boolean intersection edges need their own creation contract before selection.
[[nodiscard]] std::vector<EdgeProvenance>
extrusionEdgeOrigins(document::FeatureId extrusion, document::FeatureId sketch,
                     std::span<const EvaluatedProfileFace> profiles, const kernel::Prism& prism);
// M20: the same for a revolve; a full turn has no first or last contour.
[[nodiscard]] std::vector<EdgeProvenance>
extrusionEdgeOrigins(document::FeatureId extrusion, document::FeatureId sketch,
                     std::span<const EvaluatedProfileFace> profiles,
                     const kernel::Revolution& revolution);
} // namespace voelund::features
