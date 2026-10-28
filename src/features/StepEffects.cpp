// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "StepEffects.hpp"

#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeOperations.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <algorithm>

namespace voelund::features::detail {
namespace {

// A hole cuts; an extrusion cuts or joins (a face sketch's extrusion joins unless told).
bool cuts(const document::DocumentSnapshot& document, document::FeatureId step) {
    const auto feature =
        std::ranges::find(document.features(), step, &document::FeatureDescriptor::id);
    if (feature != document.features().end() && feature->kind == document::FeatureKind::hole)
        return true;
    const auto operation = document.extrusionParameters(step).operation;
    if (operation && *operation != document::ExtrusionOperation::join &&
        *operation != document::ExtrusionOperation::cut)
        throw GeometryEvaluationError("Only holes, joins and cuts can be repeated",
                                      "repeated-step-kind");
    return operation == document::ExtrusionOperation::cut;
}

} // namespace

std::vector<StepEffect> stepEffects(const document::DocumentSnapshot& document,
                                    std::span<const document::FeatureId> steps,
                                    document::BodyId body,
                                    const std::vector<EvaluatedFeature>& outputs, bool reference) {
    std::vector<StepEffect> result;
    for (const auto step : steps) {
        const auto& after = findFeature(outputs, step);
        const auto producer = bodyProducerBefore(document, step, body);
        if (!producer || after.bodyId != body)
            throw GeometryEvaluationError("A repeated step no longer changes the body",
                                          "repeated-step-missing");
        const auto& before = findFeature(outputs, *producer);
        const auto& from = reference ? before.referenceShape : before.shape;
        const auto& to = reference ? after.referenceShape : after.shape;
        if (cuts(document, step))
            result.push_back({kernel::ShapeOperations::cut(from, to), {}});
        else
            result.push_back({{}, kernel::ShapeOperations::cut(to, from)});
    }
    return result;
}

NamedBody repeatEffects(const NamedBody& start, std::span<const StepEffect> effects,
                        std::span<const gp_Trsf> transforms) {
    std::vector<TopoDS_Shape> removed;
    std::vector<TopoDS_Shape> added;
    for (const auto& transform : transforms)
        for (const auto& effect : effects) {
            if (!effect.removed.IsNull())
                removed.push_back(kernel::transformedCopy(effect.removed, transform));
            if (!effect.added.IsNull())
                added.push_back(kernel::transformedCopy(effect.added, transform));
        }
    NamedBody result = start;
    const auto apply = [&](const kernel::ShapeOperationResult& operation) {
        result = {operation.shape,
                  continueSupportOrigins(result.faces, operation, kernel::HistoryOperand::first),
                  continueEdgeOrigins(result.edges, operation, kernel::HistoryOperand::first)};
    };
    if (!added.empty())
        apply(kernel::ShapeOperations::fuseWithHistory(result.shape,
                                                       kernel::ShapeOperations::compound(added)));
    if (!removed.empty())
        apply(kernel::ShapeOperations::cutWithHistory(result.shape,
                                                      kernel::ShapeOperations::compound(removed)));
    return result;
}

} // namespace voelund::features::detail
