#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <span>
#include <vector>

namespace voelund::kernel {

// M25: a face a draft cannot tilt, by its index in the list given.
class DraftFaceError final : public std::runtime_error {
  public:
    explicit DraftFaceError(std::size_t face);
    [[nodiscard]] std::size_t face() const noexcept { return face_; }

  private:
    std::size_t face_;
};

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
    // M22: walls `thickness` thick, inward (outer faces stay) or outward; `openFaces` are
    // removed. Faces keep their history (HistoryOperand::first). A body that cannot take the
    // walls throws KernelError with BooleanFailure::shellFailed.
    [[nodiscard]] static ShapeOperationResult
    shellWithHistory(const TopoDS_Shape& input, std::span<const TopoDS_Shape> openFaces,
                     double thickness, bool outward);
    // M25 (DraftOperation.cpp): tilts the faces by `angle` (radians, |angle| below 90 degrees)
    // about their line on the neutral plane through `neutralOrigin`, normal to the pull
    // direction. Faces keep their history. A face that cannot be tilted throws DraftFaceError;
    // other failures KernelError.
    [[nodiscard]] static ShapeOperationResult
    draftWithHistory(const TopoDS_Shape& input, std::span<const TopoDS_Shape> faces,
                     const std::array<double, 3>& neutralOrigin,
                     const std::array<double, 3>& pullDirection, double angle);
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
