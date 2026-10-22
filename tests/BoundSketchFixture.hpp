// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <algorithm>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

namespace voelund::test {
inline document::SketchSupportBinding topBinding(const application::CadSession& session,
                                                 document::FeatureId source,
                                                 const document::SketchPlane& plane) {
    const auto evaluated =
        features::FeatureGraph{}.evaluateDetailed(session.identifiedSnapshot().snapshot());
    const auto& support =
        *std::ranges::find(evaluated->result.features, source, &features::EvaluatedFeature::id);
    const features::SupportOrigin origin{source, kernel::PrimitiveRole::boxPositiveZ};
    const auto resolved = features::resolveSupportOrigin(support.supportOrigins, origin);
    if (resolved.status != features::SupportResolutionStatus::resolved)
        throw std::runtime_error("Fixture support unavailable");
    return features::bindSketchSupport(session.documentId(), support, origin,
                                       resolved.candidates.front(), plane);
}
// These fixtures intentionally name the source box's positive-Z role.
inline void bindTop(document::ModelDocument& model, document::FeatureId sketch) {
    const auto descriptors = model.features();
    const auto source =
        *std::ranges::find(descriptors, sketch, &document::FeatureDescriptor::id)->inputFeature;
    const auto evaluated =
        features::FeatureGraph{}.evaluateDetailed(document::DocumentSnapshot::capture(model));
    const auto& support =
        *std::ranges::find(evaluated->result.features, source, &features::EvaluatedFeature::id);
    const features::SupportOrigin origin{source, kernel::PrimitiveRole::boxPositiveZ};
    const auto resolved = features::resolveSupportOrigin(support.supportOrigins, origin);
    if (resolved.status != features::SupportResolutionStatus::resolved)
        throw std::runtime_error("Fixture support unavailable");
    auto parameters = model.sketchParameters(sketch);
    parameters.supportBinding = features::bindSketchSupport(
        model.documentId(), support, origin, resolved.candidates.front(), parameters.plane);
    if (!model.setSketchParameters(sketch, parameters))
        throw std::runtime_error("Fixture binding unchanged");
}
inline void bindTop(application::CadSession& session, document::FeatureId sketch) {
    const auto inspection = session.inspectSketchReferences(sketch);
    const auto candidate =
        std::ranges::find_if(inspection.review.candidates, [&](const auto& item) {
            return item.binding.reference.origin.feature == inspection.support &&
                   item.binding.reference.origin.role == kernel::PrimitiveRole::boxPositiveZ;
        });
    if (candidate == inspection.review.candidates.end() ||
        session.repairSketchSupport(inspection, *candidate).status() !=
            application::CommandStatus::committed)
        throw std::runtime_error("Fixture support binding failed");
}
} // namespace voelund::test
