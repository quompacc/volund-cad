#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Shape.hxx>

#include <span>

namespace voelund::kernel {

class ShapeOperations final {
  public:
    [[nodiscard]] static ShapeOperationResult fuseWithHistory(const TopoDS_Shape& left,
                                                              const TopoDS_Shape& right);
    [[nodiscard]] static ShapeOperationResult cutWithHistory(const TopoDS_Shape& input,
                                                             const TopoDS_Shape& tool);
    [[nodiscard]] static ShapeOperationResult intersectWithHistory(const TopoDS_Shape& left,
                                                                   const TopoDS_Shape& right);
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
