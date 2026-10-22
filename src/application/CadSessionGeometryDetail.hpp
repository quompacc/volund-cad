// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <algorithm>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/CylinderFeature.hpp>

namespace voelund::application::detail {
inline std::size_t featureCount(std::span<const document::FeatureDescriptor> features,
                                document::FeatureKind kind) {
    std::size_t count = 0;
    for (const document::FeatureDescriptor& feature : features) {
        if (feature.kind == kind) {
            ++count;
        }
    }
    return count;
}

inline const features::EvaluatedFeature&
evaluatedFeature(std::span<const features::EvaluatedFeature> features, document::FeatureId id) {
    const auto feature = std::ranges::find(features, id, &features::EvaluatedFeature::id);
    if (feature == features.end()) {
        throw std::logic_error("Cylinder input has not been evaluated");
    }
    return *feature;
}

inline void translateCylinderBetweenInputAndReference(document::CylinderParameters& parameters,
                                                      const features::EvaluatedFeature& input,
                                                      double direction) {
    const gp_Pnt inputOrigin =
        features::CylinderFeature::attachmentOrigin(input.shape, parameters.height);
    const gp_Pnt referenceOrigin =
        features::CylinderFeature::attachmentOrigin(input.referenceShape, parameters.height);
    parameters.placement.translationX += direction * (inputOrigin.X() - referenceOrigin.X());
    parameters.placement.translationY += direction * (inputOrigin.Y() - referenceOrigin.Y());
    parameters.placement.translationZ += direction * (inputOrigin.Z() - referenceOrigin.Z());
}

} // namespace voelund::application::detail
