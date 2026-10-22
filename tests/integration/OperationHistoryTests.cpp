// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/kernel/OperationHistoryDetail.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

using namespace voelund::kernel;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double actual, double expected, const char* message) {
    require(std::abs(actual - expected) < 1.e-7, message);
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}
double area(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}
TopoDS_Shape role(const ShapeOperationResult& result, PrimitiveRole wanted) {
    for (const auto& origin : result.origins)
        if (origin.role == wanted)
            return origin.shape;
    throw std::runtime_error("Primitive role missing");
}
const SubshapeHistory& history(const ShapeOperationResult& result, const TopoDS_Shape& source,
                               HistoryOperand operand = HistoryOperand::first) {
    for (const auto& entry : result.history)
        if (entry.operand == operand && entry.source.IsSame(source))
            return entry;
    throw std::runtime_error("Input subshape missing from history");
}
TopoDS_Shape commonEdge(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    for (TopExp_Explorer a(first, TopAbs_EDGE); a.More(); a.Next())
        for (TopExp_Explorer b(second, TopAbs_EDGE); b.More(); b.Next())
            if (a.Current().IsSame(b.Current()))
                return a.Current();
    throw std::runtime_error("Semantic faces have no shared edge");
}
TopoDS_Shape tool(double x, double y, double z, double dx, double dy, double dz) {
    return ShapeFactory::applyTransform(ShapeFactory::makeBox({dx, dy, dz}), 0, 0, 0,
                                        {x, y, z, {}, {}, {}});
}
void complete(const ShapeOperationResult& result) {
    require(BRepCheck_Analyzer(result.shape).IsValid(), "Invalid result BRep");
    for (const auto& entry : result.history) {
        require(entry.coverage == HistoryCoverage::complete, "Unexpected history gap");
        for (const auto& shape : entry.modified)
            require(!shape.IsNull() && shape.ShapeType() == entry.source.ShapeType(),
                    "Wrong modified topology");
    }
}
void cutFixtures() {
    const auto box = ShapeFactory::makeBoxWithHistory({10, 10, 10});
    const auto top = role(box, PrimitiveRole::boxPositiveZ);
    const auto edge = commonEdge(top, role(box, PrimitiveRole::boxNegativeY));
    const auto cut = ShapeOperations::cutWithHistory(box.shape, tool(8, -1, -1, 3, 12, 12));
    complete(cut);
    const auto& faceHistory = history(cut, top);
    require(faceHistory.modified.size() == 1 && !faceHistory.deleted && !faceHistory.unchanged,
            "Side cut must retain one modified top face");
    near(area(faceHistory.modified.front()), 80, "Modified top area");
    const auto& edgeHistory = history(cut, edge);
    require(edgeHistory.modified.size() == 1 && !edgeHistory.deleted, "Trimmed edge history");
    GProp_GProps length;
    BRepGProp::LinearProperties(edgeHistory.modified.front(), length);
    near(length.Mass(), 8, "Trimmed edge length");
    near(volume(cut.shape), 800, "Side cut volume");
    near(volume(box.shape), 1000, "Boolean mutated input");
    const auto split = ShapeOperations::cutWithHistory(box.shape, tool(4, -1, 9, 2, 12, 2));
    complete(split);
    const auto& splitHistory = history(split, top);
    require(splitHistory.modified.size() == 2 && !splitHistory.deleted, "Split must be ambiguous");
    for (const auto& face : splitHistory.modified)
        near(area(face), 40, "Split top area");
    require(!splitHistory.generated.empty(), "Cut must expose generated intersection geometry");
    for (const auto& generated : splitHistory.generated)
        require(generated.ShapeType() != TopAbs_FACE,
                "Generated edge must not replace source face");
    near(volume(split.shape), 980, "Notch volume");
    const auto removed = ShapeOperations::cutWithHistory(box.shape, tool(-1, -1, 9, 12, 12, 2));
    complete(removed);
    const auto& deleted = history(removed, top);
    require(deleted.deleted && deleted.modified.empty() && !deleted.unchanged,
            "Removed top must stay Missing despite new coplanar-normal face");
    near(volume(removed.shape), 900, "Removed slab volume");
    std::cout
        << "cut: face Resolved area80, edge length8, split Ambiguous 2x40, deletion Missing\n";
}
void primitiveAndTransformFixtures() {
    auto box = ShapeFactory::makeBoxWithHistory({10, 20, 30});
    require(box.origins.size() == 6, "Box roles");
    for (const auto& origin : box.origins)
        require(origin.shape.ShapeType() == TopAbs_FACE, "Box face");
    const auto top = role(box, PrimitiveRole::boxPositiveZ);
    const auto rotated = ShapeFactory::applyTransformWithHistory(
        box.shape, 0, 0, 0, {5, 6, 7, {}, Radians{std::numbers::pi / 2}, {}});
    box = {}; // Result and history retain their own OCCT handles after original owner dies.
    complete(rotated);
    const auto& mapped = history(rotated, top);
    require(mapped.modified.size() == 1, "Rotation history must be unique");
    BRepAdaptor_Surface surface(TopoDS::Face(mapped.modified.front()));
    near(std::abs(surface.Plane().Axis().Direction().X()), 1, "Rotated normal");
    near(surface.Plane().Location().X(), 35, "Rotated translated top plane");
    near(volume(rotated.shape), 6000, "Rigid volume");
    const auto cylinder = ShapeFactory::makeCylinderWithHistory({2, 5, 0, 0, 0, 1, 0, 0});
    require(cylinder.origins.size() == 3, "Cylinder roles");
    near(area(role(cylinder, PrimitiveRole::cylinderEnd)), 4 * std::numbers::pi, "Cylinder cap");
    near(area(role(cylinder, PrimitiveRole::cylinderSide)), 20 * std::numbers::pi, "Cylinder side");
    std::cout << "primitive roles and rotation: retained after builder/source-owner destruction\n";
}
void otherBooleanFixtures() {
    const auto box = ShapeFactory::makeBoxWithHistory({10, 10, 10});
    const auto top = role(box, PrimitiveRole::boxPositiveZ);
    const auto overlap = tool(5, 0, 0, 10, 10, 10);
    const auto united = ShapeOperations::fuseWithHistory(box.shape, overlap);
    complete(united);
    near(volume(united.shape), 1500, "Fuse volume");
    const auto& fusedTop = history(united, top);
    require(!fusedTop.deleted && (fusedTop.unchanged || !fusedTop.modified.empty()),
            "Fuse lineage");
    const auto intersected = ShapeOperations::intersectWithHistory(box.shape, overlap);
    complete(intersected);
    near(volume(intersected.shape), 500, "Common volume");
    require(history(intersected, top).modified.size() == 1, "Common top history");
    // A separate coplanar body is distinguished by topology identity, never plane distance.
    const auto distant = tool(20, 0, 0, 10, 10, 10);
    const auto compound = ShapeOperations::compound(box.shape, distant);
    const auto local = ShapeOperations::cutWithHistory(compound, tool(8, -1, -1, 3, 12, 12));
    complete(local);
    near(volume(local.shape), 1800, "Independent coplanar body preserved");
    require(history(local, top).modified.size() == 1, "Coplanar source face did not stay distinct");
    // Same input handle in both operands must have separately queryable provenance.
    const auto same = ShapeOperations::intersectWithHistory(box.shape, box.shape);
    complete(same);
    require(history(same, top).unchanged && history(same, top, HistoryOperand::second).unchanged,
            "Operand provenance lost");
    std::cout << "fuse/common, coplanar bodies, unchanged topology and operand provenance passed\n";
}
void unavailableHistoryFixture() {
    const auto box = ShapeFactory::makeBox({10, 10, 10});
    const auto cutter = tool(8, -1, -1, 3, 12, 12);
    BRepAlgoAPI_Cut operation;
    detail::configureBoolean(operation, box, cutter);
    operation.SetToFillHistory(false);
    operation.Build();
    const auto result = detail::booleanResult(operation, box, cutter);
    require(!result.history.empty(), "Missing history must retain source entries");
    for (const auto& entry : result.history)
        require(entry.coverage == HistoryCoverage::unavailable,
                "Disabled history must not be invented from traversal order");
    std::cout << "disabled OCCT history: all source entries explicitly unavailable\n";
}
} // namespace
int main() {
    try {
        cutFixtures();
        primitiveAndTransformFixtures();
        otherBooleanFixtures();
        unavailableHistoryFixture();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
