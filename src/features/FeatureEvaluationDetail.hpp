#pragma once
#include <voelund/document/FeatureId.hpp>
#include <voelund/features/FeatureGraph.hpp>

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

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
// M19/M10 (DatumEvaluation.cpp): the line of a base axis or of a body edge as `feature` sees
// it: a straight edge itself, a circular edge's axis through its centre.
struct AxisLine final {
    gp_Pnt origin;
    gp_Dir direction;
};
[[nodiscard]] AxisLine datumAxisLine(const document::DocumentSnapshot& document,
                                     document::FeatureId feature,
                                     const document::DatumAxisParameters& parameters,
                                     const std::vector<EvaluatedFeature>& outputs);
// M10 (PatternEvaluation.cpp): a pattern passes its body on (new bodies) or joins the copies
// into it; every copy of a new-body pattern is a body of its own.
void evaluatePattern(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies);
void evaluatePatternCopy(const document::DocumentSnapshot& document,
                         const document::FeatureDescriptor& descriptor,
                         std::vector<EvaluatedFeature>& outputs,
                         std::vector<EvaluatedBody>& bodies,
                         std::vector<EvaluatedBody>& referenceBodies);
// M11 (MoveEvaluation.cpp): moves and turns a body about its stored pivot.
void evaluateMove(const document::DocumentSnapshot& document,
                  const document::FeatureDescriptor& descriptor,
                  std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                  std::vector<EvaluatedBody>& referenceBodies);
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
