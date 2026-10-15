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
// M05 (FilletEvaluation.cpp): rounds edges of a body named by their adjacent faces.
void evaluateFillet(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies);
// M07: the same for a symmetric chamfer.
void evaluateChamfer(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies);
// S17 (SketchProjection.cpp): projected elements follow their body edges.
void refreshSketchProjections(document::SketchParameters& sketch,
                              const document::DocumentSnapshot& document,
                              document::FeatureId sketchId,
                              const std::vector<EvaluatedFeature>& outputs);
// M19 (DatumEvaluation.cpp): construction planes and axes; no body.
void evaluateDatumPlane(const document::DocumentSnapshot& document,
                        const document::FeatureDescriptor& descriptor,
                        std::vector<EvaluatedFeature>& outputs);
void evaluateDatumAxis(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs);
// M22 (ShellEvaluation.cpp): walls of one thickness with named faces opened.
// M25 (DraftEvaluation.cpp): tilts named faces of a body.
void evaluateDraft(const document::DocumentSnapshot& document,
                   const document::FeatureDescriptor& descriptor,
                   std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                   std::vector<EvaluatedBody>& referenceBodies);
void evaluateShell(const document::DocumentSnapshot& document,
                   const document::FeatureDescriptor& descriptor,
                   std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                   std::vector<EvaluatedBody>& referenceBodies);
// M21 (CombineEvaluation.cpp): a Boolean of whole bodies into the target body.
void evaluateCombine(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies);
// M09 (MirrorEvaluation.cpp): the mirror image of a body, as a body or joined to it.
void evaluateMirror(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies);
// M08 (HoleEvaluation.cpp): a cylindrical hole into a named planar face.
void evaluateHole(const document::DocumentSnapshot& document,
                  const document::FeatureDescriptor& descriptor,
                  std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                  std::vector<EvaluatedBody>& referenceBodies);
} // namespace voelund::features::detail
