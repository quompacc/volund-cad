#pragma once
#include <voelund/features/FeatureGraph.hpp>

namespace voelund::features {
// Caller supplies a face actually selected from this current evaluated output.
[[nodiscard]] document::SketchSupportBinding
bindSketchSupport(document::DocumentId document, const EvaluatedFeature& support,
                  SupportOrigin origin, const TopoDS_Shape& selectedFace,
                  const document::SketchPlane& sketchPlane);
// T01: a planar face of a current output that a sketch can name, in the frame an
// interactive pick of that face would give the sketch. Bounds are the face extent in that
// frame (mm), so a caller can place sketch coordinates without seeing the model.
struct SupportFaceFrame final {
    SupportOrigin origin;
    TopoDS_Shape face;
    document::SketchPlane plane;
    double minX, minY, maxX, maxY;
    bool secondary{false}; // T05: a second name of a merged face; picks take the first
};
// Planar faces with complete history and exactly one current face per origin; a face that
// more than one origin claims is left out because a binding to it would be ambiguous.
[[nodiscard]] std::vector<SupportFaceFrame> supportFaceFrames(const EvaluatedFeature& support);
// Missing/ambiguous/nonplanar support throws a localized GeometryEvaluationError.
// No legacy fallback and no mutation of the persisted last plane on failure.
void resolveBoundSketchSupport(document::SketchParameters& sketch, const EvaluatedFeature& support);
// Shared boundary and ancestry validation for evaluation and interactive projection.
[[nodiscard]] gp_Pnt resolveBoundEdgePoint(const document::SketchSupportBinding& binding,
                                           const EvaluatedFeature& support,
                                           const document::ProjectedEdgeReference& reference);
} // namespace voelund::features
