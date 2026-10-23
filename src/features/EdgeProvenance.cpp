// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <voelund/features/EdgeProvenance.hpp>
#include <voelund/kernel/EdgeQueries.hpp>

namespace voelund::features {
namespace {
bool contains(const TopoDS_Shape& shape, const TopoDS_Shape& subshape) {
    for (TopExp_Explorer it(shape, subshape.ShapeType()); it.More(); it.Next())
        if (it.Current().IsSame(subshape))
            return true;
    return false;
}
void appendUnique(std::vector<TopoDS_Edge>& edges, const TopoDS_Shape& edge) {
    if (edge.IsNull() || edge.ShapeType() != TopAbs_EDGE)
        throw std::logic_error("Edge provenance needs an edge");
    if (std::ranges::none_of(edges, [&](const auto& e) { return e.IsSame(edge); }))
        edges.push_back(TopoDS::Edge(edge.Oriented(TopAbs_FORWARD)));
}
bool sameCurve(const TopoDS_Edge& a, const TopoDS_Edge& b) {
    return kernel::sameUnderlyingCurve(a, b);
}
double normalized(const EdgeProvenance& origin, double parameter) {
    return (parameter - origin.startParameter) / (origin.endParameter - origin.startParameter);
}
EdgeResolutionStatus status(const EdgeProvenance& item) {
    if (item.coverage != kernel::HistoryCoverage::complete || !item.parameterMapping)
        return EdgeResolutionStatus::unavailable;
    if (item.edges.empty())
        return EdgeResolutionStatus::missing;
    if (item.edges.size() != 1)
        return EdgeResolutionStatus::ambiguous;
    return EdgeResolutionStatus::resolved;
}
} // namespace
std::vector<EdgeProvenance> primitiveEdgeOrigins(document::FeatureId feature,
                                                 const kernel::ShapeOperationResult& primitive) {
    std::vector<EdgeProvenance> result;
    for (const auto& first : primitive.origins)
        for (const auto& second : primitive.origins) {
            if (first.role >= second.role)
                continue;
            std::vector<TopoDS_Edge> common;
            for (TopExp_Explorer it(first.shape, TopAbs_EDGE); it.More(); it.Next())
                if (contains(second.shape, it.Current()))
                    appendUnique(common, it.Current());
            if (common.empty())
                continue;
            if (common.size() != 1)
                throw std::logic_error("Primitive face pair has multiple edges");
            const auto& edge = common.front();
            const auto range = kernel::curveRange(edge);
            double start = range.first, end = range.last;
            if (!std::isfinite(start) || !std::isfinite(end) || start >= end)
                throw std::logic_error("Primitive edge has invalid parameter domain");
            // Box direction comes from its free local axis's negative endpoint face.
            // Circular seam and direction come from the primitive's analytic frame.
            if (second.role <= kernel::PrimitiveRole::boxPositiveZ) {
                const int freeAxis =
                    3 - static_cast<int>(first.role) / 2 - static_cast<int>(second.role) / 2;
                const auto role = static_cast<kernel::PrimitiveRole>(2 * freeAxis);
                const auto negative =
                    std::ranges::find(primitive.origins, role, &kernel::PrimitiveOrigin::role);
                if (negative == primitive.origins.end())
                    throw std::logic_error("Missing box endpoint role");
                if (!contains(negative->shape, TopExp::FirstVertex(edge))) {
                    if (!contains(negative->shape, TopExp::LastVertex(edge)))
                        throw std::logic_error("Unproven box edge direction");
                    std::swap(start, end);
                }
            }
            result.push_back({{feature, first.role, second.role}, edge, start, end, common});
        }
    return result;
}
std::vector<EdgeProvenance> continueEdgeOrigins(std::span<const EdgeProvenance> previous,
                                                const kernel::ShapeOperationResult& operation,
                                                kernel::HistoryOperand operand) {
    std::vector<EdgeProvenance> result;
    for (const auto& item : previous) {
        EdgeProvenance next{item.origin, item.source,   item.startParameter,  item.endParameter,
                            {},          item.coverage, item.parameterMapping};
        for (const auto& edge : item.edges) {
            const auto found = std::ranges::find_if(operation.history, [&](const auto& h) {
                return h.operand == operand && h.source.IsSame(edge);
            });
            if (found == operation.history.end() ||
                found->coverage != kernel::HistoryCoverage::complete) {
                next.coverage = kernel::HistoryCoverage::unavailable;
                continue;
            }
            if (found->unchanged)
                appendUnique(next.edges, edge);
            for (const auto& mapped : found->modified) {
                appendUnique(next.edges, mapped);
                if (!sameCurve(item.source, TopoDS::Edge(mapped)))
                    next.parameterMapping = false;
            }
        }
        result.push_back(std::move(next));
    }
    return result;
}
EdgePointResolution resolveEdgePoint(std::span<const EdgeProvenance> origins,
                                     const EdgeOrigin& origin, EdgePointSelection selection) {
    if (selection.kind != EdgePointKind::start && selection.kind != EdgePointKind::end &&
        selection.kind != EdgePointKind::parameter)
        throw std::invalid_argument("Invalid edge point kind");
    if (!std::isfinite(selection.parameter) || selection.parameter < 0 || selection.parameter > 1)
        throw std::invalid_argument("Curve parameter must be in [0,1]");
    const double t = selection.kind == EdgePointKind::start ? 0
                     : selection.kind == EdgePointKind::end ? 1
                                                            : selection.parameter;
    // R10b: a split element has one entry per fragment, all on its whole source domain.
    // The point belongs to the fragment containing it; nothing is clamped or reinterpreted.
    std::optional<EdgeResolutionStatus> failure;
    for (const EdgeProvenance& item : origins) {
        if (!(item.origin == origin))
            continue;
        const auto state = status(item);
        if (state != EdgeResolutionStatus::resolved) {
            failure = failure.value_or(state);
            continue;
        }
        const double u = item.startParameter + t * (item.endParameter - item.startParameter);
        const auto current = kernel::curveRange(item.edges.front());
        if (u < current.first || u > current.last) {
            failure = failure.value_or(EdgeResolutionStatus::parameterOutside);
            continue;
        }
        return {EdgeResolutionStatus::resolved, kernel::pointOnEdge(item.source, u)};
    }
    return {failure.value_or(EdgeResolutionStatus::missing)};
}
std::vector<EdgeDisplaySegment> tessellateEdge(const EdgeProvenance& origin, unsigned divisions) {
    if (divisions == 0 || divisions > 4096)
        throw std::invalid_argument("Invalid edge display density");
    if (status(origin) != EdgeResolutionStatus::resolved)
        return {};
    const auto& edge = origin.edges.front();
    const auto range = kernel::curveRange(edge);
    const double low = std::max(std::min(origin.startParameter, origin.endParameter), range.first);
    const double high = std::min(std::max(origin.startParameter, origin.endParameter), range.last);
    if (low >= high)
        return {};
    const double start = origin.startParameter < origin.endParameter ? low : high;
    const double end = origin.startParameter < origin.endParameter ? high : low;
    std::vector<EdgeDisplaySegment> result;
    for (unsigned i = 0; i < divisions; ++i) {
        const double a = start + (end - start) * i / divisions,
                     b = start + (end - start) * (i + 1) / divisions;
        result.push_back({origin.origin, normalized(origin, a), normalized(origin, b),
                          kernel::pointOnEdge(edge, a), kernel::pointOnEdge(edge, b)});
    }
    return result;
}
} // namespace voelund::features
