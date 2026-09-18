#pragma once
#include <voelund/features/FeatureGraph.hpp>

namespace voelund::features {
// Caller supplies a face actually selected from this current evaluated output.
[[nodiscard]] document::SketchSupportBinding
bindSketchSupport(document::DocumentId document, const EvaluatedFeature& support,
                  SupportOrigin origin, const TopoDS_Shape& selectedFace,
                  const document::SketchPlane& sketchPlane);
// Missing/ambiguous/nonplanar support throws a localized GeometryEvaluationError.
// No legacy fallback and no mutation of the persisted last plane on failure.
void resolveBoundSketchSupport(document::SketchParameters& sketch, const EvaluatedFeature& support);
// Shared boundary and ancestry validation for evaluation and interactive projection.
[[nodiscard]] gp_Pnt resolveBoundEdgePoint(const document::SketchSupportBinding& binding,
                                           const EvaluatedFeature& support,
                                           const document::ProjectedEdgeReference& reference);
} // namespace voelund::features
