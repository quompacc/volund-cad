// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>

// M23/N11c: the path of a sweep or of a pattern along a curve, the chain of elements of an
// earlier sketch in any order.
namespace voelund::features::detail {

TopoDS_Wire sketchPathWire(const document::SweepPath& path,
                           const std::vector<EvaluatedFeature>& outputs,
                           const std::string& prefix) {
    const auto& pathOutput = findFeature(outputs, path.sketch);
    if (!pathOutput.resolvedSketch)
        throw GeometryEvaluationError("The path sketch has no geometry", prefix + "-path-missing");
    const auto elements = evaluateSketchElements(*pathOutput.resolvedSketch);
    std::vector<TopoDS_Edge> edges;
    for (const auto id : path.elements) {
        const auto element = std::ranges::find(elements, id, &EvaluatedSketchElement::id);
        if (element == elements.end())
            throw GeometryEvaluationError("A path element is missing", prefix + "-path-missing",
                                          {id.value()});
        for (TopExp_Explorer it(element->shape, TopAbs_EDGE); it.More(); it.Next())
            edges.push_back(TopoDS::Edge(it.Current()));
    }
    // The chain in any order: each pass adds the edges that touch the wire so far.
    const auto wire = kernel::chainedWire(edges);
    if (!wire)
        throw GeometryEvaluationError("The path is not one connected chain",
                                      prefix + "-path-disconnected");
    return *wire;
}

} // namespace voelund::features::detail
