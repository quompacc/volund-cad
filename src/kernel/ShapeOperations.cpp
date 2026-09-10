#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <array>
#include <stdexcept>

namespace voelund::kernel {
namespace {

int solidCountOf(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

} // namespace

TopoDS_Shape ShapeOperations::fuse(const TopoDS_Shape& left, const TopoDS_Shape& right)
{
    BRepAlgoAPI_Fuse operation(left, right);
    operation.Build();
    if (!operation.IsDone()) {
        throw std::runtime_error("OCCT failed to join the tool with its input body");
    }
    const TopoDS_Shape result = operation.Shape();
    if (solidCountOf(result) != 1) {
        throw std::runtime_error("Join requires the tool to intersect its input body");
    }
    return result;
}

TopoDS_Shape ShapeOperations::cut(const TopoDS_Shape& input, const TopoDS_Shape& tool)
{
    static_cast<void>(intersect(input, tool));
    BRepAlgoAPI_Cut operation(input, tool);
    operation.Build();
    if (!operation.IsDone()) {
        throw std::runtime_error("OCCT failed to cut the tool from its input body");
    }
    const TopoDS_Shape result = operation.Shape();
    if (solidCountOf(result) == 0) {
        throw std::runtime_error("Cut removed the entire input body");
    }
    return result;
}

TopoDS_Shape ShapeOperations::intersect(
    const TopoDS_Shape& left,
    const TopoDS_Shape& right)
{
    BRepAlgoAPI_Common operation(left, right);
    operation.Build();
    if (!operation.IsDone()) {
        throw std::runtime_error("OCCT failed to intersect the tool with its input body");
    }
    const TopoDS_Shape result = operation.Shape();
    if (solidCountOf(result) == 0) {
        throw std::runtime_error("Intersect requires the tool to overlap its input body");
    }
    return result;
}

TopoDS_Shape ShapeOperations::compound(const TopoDS_Shape& first, const TopoDS_Shape& second)
{
    const std::array shapes{first, second};
    return compound(shapes);
}

TopoDS_Shape ShapeOperations::compound(std::span<const TopoDS_Shape> shapes)
{
    BRep_Builder builder;
    TopoDS_Compound result;
    builder.MakeCompound(result);
    for (const TopoDS_Shape& shape : shapes) {
        builder.Add(result, shape);
    }
    return result;
}

} // namespace voelund::kernel
