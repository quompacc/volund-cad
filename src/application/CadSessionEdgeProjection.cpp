// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <gp_Vec.hxx>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

namespace voelund::application {
void CadSession::resolveSketchSupport(document::FeatureId support,
                                      document::SketchParameters& sketch) const {
    if (!evaluation_ || !sketch.supportBinding ||
        sketch.supportBinding->reference.document != documentId())
        throw std::invalid_argument("Sketch support namespace is not current");
    const auto& outputs = evaluation_->result.features;
    const auto found = std::ranges::find(outputs, support, &features::EvaluatedFeature::id);
    if (found == outputs.end())
        throw std::invalid_argument("Sketch support is not current");
    features::resolveBoundSketchSupport(sketch, *found);
}

SketchSupportProjection
CadSession::projectSketchSupport(document::FeatureId support,
                                 const document::SketchSupportBinding& binding,
                                 unsigned divisions) const {
    if (divisions < 1 || divisions > 4096)
        throw std::invalid_argument("Projection divisions must be in [1,4096]");
    document::SketchParameters sketch;
    sketch.supportBinding = binding;
    resolveSketchSupport(support, sketch);
    const auto& outputs = evaluation_->result.features;
    const auto& evaluated = *std::ranges::find(outputs, support, &features::EvaluatedFeature::id);
    SketchSupportProjection result{sketch.plane, {}, {}};
    const auto& p = sketch.plane;
    const gp_Pnt origin(p.originX, p.originY, p.originZ);
    const gp_Vec x(p.xDirectionX, p.xDirectionY, p.xDirectionZ);
    const auto y = gp_Vec(p.normalX, p.normalY, p.normalZ).Crossed(x);
    for (const auto& edge : evaluated.edgeOrigins) {
        const auto segments = features::tessellateEdge(edge, divisions);
        if (segments.empty())
            continue;
        // Validate face membership even if the original midpoint was trimmed away.
        document::ProjectedEdgeReference ref{
            documentId(),
            binding.reference.body,
            edge.origin,
            {document::EdgePointKind::parameter, segments.front().firstParameter}};
        try {
            (void)features::resolveBoundEdgePoint(binding, evaluated, ref);
        } catch (const features::GeometryEvaluationError&) {
            continue; // Unknown, ambiguous or non-boundary ancestry is not selectable.
        }
        result.segments.insert(result.segments.end(), segments.begin(), segments.end());
        for (const auto selection :
             {document::EdgePointSelection{document::EdgePointKind::start, 0},
              document::EdgePointSelection{document::EdgePointKind::parameter, .5},
              document::EdgePointSelection{document::EdgePointKind::end, 0}}) {
            ref.selection = selection;
            try {
                const auto world = features::resolveBoundEdgePoint(binding, evaluated, ref);
                const gp_Vec offset(origin, world);
                result.points.push_back({ref, {offset.Dot(x), offset.Dot(y)}, world});
            } catch (const features::GeometryEvaluationError&) {
                // Trimmed original parameters have no substitute point.
            }
        }
    }
    return result;
}
} // namespace voelund::application
