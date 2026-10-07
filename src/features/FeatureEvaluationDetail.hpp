#pragma once
#include <voelund/document/FeatureId.hpp>
#include <voelund/features/FeatureGraph.hpp>

#include <vector>

namespace voelund::features::detail {
// Private deterministic fault seam. No product configuration enables it.
// Thread-local so one test's evaluation cannot affect another thread.
inline thread_local void (*beforeFeatureEvaluation)(document::FeatureId) = nullptr;

// Shared by the feature evaluators of FeatureGraph.cpp and ExtrusionEvaluation.cpp.
[[nodiscard]] const EvaluatedFeature& findFeature(const std::vector<EvaluatedFeature>& outputs,
                                                  document::FeatureId featureId);
// The body `featureId` produced last; throws when a later feature changed it already.
[[nodiscard]] EvaluatedBody& findBody(std::vector<EvaluatedBody>& bodies,
                                      const std::vector<EvaluatedFeature>& outputs,
                                      document::FeatureId featureId);
void evaluateExtrusion(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies);
// T04 (FaceExtrusionEvaluation.cpp): an extrusion whose profile is a body face.
void evaluateFaceExtrusion(const document::DocumentSnapshot& document,
                           const document::FeatureDescriptor& descriptor,
                           const document::ExtrusionParameters& parameters,
                           std::vector<EvaluatedFeature>& outputs,
                           std::vector<EvaluatedBody>& bodies,
                           std::vector<EvaluatedBody>& referenceBodies);
} // namespace voelund::features::detail
