#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <span>
#include <vector>

namespace voelund::kernel {

class ShapeOperations final {
  public:
    [[nodiscard]] static ShapeOperationResult fuseWithHistory(const TopoDS_Shape& left,
                                                              const TopoDS_Shape& right);
    [[nodiscard]] static ShapeOperationResult cutWithHistory(const TopoDS_Shape& input,
                                                             const TopoDS_Shape& tool);
    [[nodiscard]] static ShapeOperationResult intersectWithHistory(const TopoDS_Shape& left,
                                                                   const TopoDS_Shape& right);
    // M05: rounds the edges of a solid with one constant radius. The edges' faces are
    // modified, each rounded edge generates its fillet face (HistoryOperand::first). A radius
    // the edges cannot take throws KernelError with BooleanFailure::filletFailed.
    [[nodiscard]] static ShapeOperationResult filletWithHistory(const TopoDS_Shape& input,
                                                                std::span<const TopoDS_Edge> edges,
                                                                double radius);
    // M05: after a failed fillet, the edges that fail on their own (indices into `edges`);
    // all of them when only the combination fails.
    [[nodiscard]] static std::vector<std::size_t>
    failingFilletEdges(const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                       double radius);
    // M07: a symmetric chamfer with one distance on each face, the same contract as above
    // (BooleanFailure::chamferFailed).
    [[nodiscard]] static ShapeOperationResult chamferWithHistory(const TopoDS_Shape& input,
                                                                 std::span<const TopoDS_Edge> edges,
                                                                 double distance);
    [[nodiscard]] static std::vector<std::size_t>
    failingChamferEdges(const TopoDS_Shape& input, std::span<const TopoDS_Edge> edges,
                        double distance);
    // Invalid inputs and failed OCCT operations throw KernelError. Its operation
    // identifies the public entry point, including failures of internal checks.
    [[nodiscard]] static TopoDS_Shape fuse(const TopoDS_Shape& left, const TopoDS_Shape& right);
    [[nodiscard]] static TopoDS_Shape cut(const TopoDS_Shape& input, const TopoDS_Shape& tool);
    [[nodiscard]] static TopoDS_Shape intersect(const TopoDS_Shape& left,
                                                const TopoDS_Shape& right);
    [[nodiscard]] static TopoDS_Shape compound(const TopoDS_Shape& first,
                                               const TopoDS_Shape& second);
    [[nodiscard]] static TopoDS_Shape compound(std::span<const TopoDS_Shape> shapes);
};

} // namespace voelund::kernel
