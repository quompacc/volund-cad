#pragma once

#include <TopoDS_Shape.hxx>

#include <span>

namespace voelund::kernel {

class ShapeOperations final {
public:
    [[nodiscard]] static TopoDS_Shape fuse(
        const TopoDS_Shape& left,
        const TopoDS_Shape& right);
    [[nodiscard]] static TopoDS_Shape cut(
        const TopoDS_Shape& input,
        const TopoDS_Shape& tool);
    [[nodiscard]] static TopoDS_Shape intersect(
        const TopoDS_Shape& left,
        const TopoDS_Shape& right);
    [[nodiscard]] static TopoDS_Shape compound(
        const TopoDS_Shape& first,
        const TopoDS_Shape& second);
    [[nodiscard]] static TopoDS_Shape compound(std::span<const TopoDS_Shape> shapes);
};

} // namespace voelund::kernel
