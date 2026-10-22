// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <voelund/application/CadSession.hpp>
namespace voelund::application {
SketchReferenceInspection CadSession::inspectSketchReferences(document::FeatureId sketch) const {
    const auto descriptors = document_.features();
    const auto found = std::ranges::find(descriptors, sketch, &document::FeatureDescriptor::id);
    if (found == descriptors.end() || found->kind != document::FeatureKind::sketch ||
        !found->inputFeature)
        throw std::invalid_argument("Reference inspection requires a supported sketch");
    const features::EvaluatedFeature* support = nullptr;
    if (evaluation_) {
        const auto& outputs = evaluation_->result.features;
        const auto item =
            std::ranges::find(outputs, *found->inputFeature, &features::EvaluatedFeature::id);
        if (item != outputs.end())
            support = &*item;
    }
    return {documentId(), sceneStatus().document, sketch, *found->inputFeature,
            features::reviewSketchReferences(documentId(), document_.sketchParameters(sketch),
                                             support)};
}
} // namespace voelund::application
