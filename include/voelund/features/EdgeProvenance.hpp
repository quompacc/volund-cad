#pragma once
#include <TopoDS_Edge.hxx>
#include <gp_Pnt.hxx>
#include <optional>
#include <span>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/OperationHistory.hpp>

namespace voelund::features {
using document::ContourEdgeOrigin;
using document::ContourRole;
using document::ContourStage;
using document::EdgeOrigin;
using document::EdgePointKind;
using document::EdgePointSelection;
using document::PrimitiveEdgeOrigin;
struct EdgeProvenance final {
    EdgeOrigin origin;
    TopoDS_Edge source;
    double startParameter;
    double endParameter;
    std::vector<TopoDS_Edge> edges;
    kernel::HistoryCoverage coverage{kernel::HistoryCoverage::complete};
    // History may prove topology without proving the analytic parameter mapping.
    bool parameterMapping{true};
};
enum class EdgeResolutionStatus { resolved, missing, ambiguous, unavailable, parameterOutside };
struct EdgePointResolution final {
    EdgeResolutionStatus status;
    std::optional<gp_Pnt> point{};
};
struct EdgeDisplaySegment final {
    EdgeOrigin origin;
    double firstParameter;
    double secondParameter;
    gp_Pnt first;
    gp_Pnt second;
};
[[nodiscard]] std::vector<EdgeProvenance>
primitiveEdgeOrigins(document::FeatureId feature, const kernel::ShapeOperationResult& primitive);
[[nodiscard]] std::vector<EdgeProvenance>
continueEdgeOrigins(std::span<const EdgeProvenance> previous,
                    const kernel::ShapeOperationResult& operation,
                    kernel::HistoryOperand operand = kernel::HistoryOperand::first);
[[nodiscard]] EdgePointResolution resolveEdgePoint(std::span<const EdgeProvenance> origins,
                                                   const EdgeOrigin& origin,
                                                   EdgePointSelection selection);
// Display samples retain analytic parameters; segment indices are never identity.
[[nodiscard]] std::vector<EdgeDisplaySegment> tessellateEdge(const EdgeProvenance& origin,
                                                             unsigned divisions);
} // namespace voelund::features
