// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/Interference.hpp>
#include <voelund/kernel/Measurement.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// N08: two overlapping boxes interfere by exactly their common volume; touching or separate
// bodies do not. Points: a plate's corners, an edge middle and a circle centre measure exactly.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(b)); }
TopoDS_Shape box(const gp_Pnt& corner, double dx, double dy, double dz) {
    return BRepPrimAPI_MakeBox(corner, dx, dy, dz).Shape();
}
TopoDS_Shape vertex(double x, double y, double z) {
    return BRepBuilderAPI_MakeVertex(gp_Pnt(x, y, z)).Vertex();
}
std::array<double, 3> pointOf(const TopoDS_Shape& shape) {
    const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(shape));
    return {point.X(), point.Y(), point.Z()};
}

void interference() {
    const std::array<TopoDS_Shape, 4> bodies{
        box(gp_Pnt(0, 0, 0), 20, 20, 20), box(gp_Pnt(10, 10, 10), 20, 20, 20),
        box(gp_Pnt(30, 0, 0), 10, 10, 10), box(gp_Pnt(40, 0, 0), 5, 5, 5)};
    const auto found = kernel::findInterferences(bodies);
    // 0 and 1 overlap by 10³; 1 and 2 overlap in x 30..30 only (touch); 2 and 3 touch at x 40.
    require(found.size() == 1 && found[0].first == 0 && found[0].second == 1 &&
                near(found[0].volume, 1000),
            "Only the overlapping pair, with exactly 1000 mm³");
    const std::array<TopoDS_Shape, 2> apart{box(gp_Pnt(0, 0, 0), 5, 5, 5),
                                            box(gp_Pnt(10, 0, 0), 5, 5, 5)};
    require(kernel::findInterferences(apart).empty(), "Separate bodies do not interfere");
}

// The plate's face through (20, 10, 10) that misses (20, 10, 0): its top.
TopoDS_Shape topFace(const TopoDS_Shape& plate) {
    for (TopExp_Explorer explorer(plate, TopAbs_FACE); explorer.More(); explorer.Next())
        if (kernel::measureBetween(explorer.Current(), vertex(20, 10, 10)).distance < 1e-9 &&
            kernel::measureBetween(explorer.Current(), vertex(20, 10, 0)).distance > 1)
            return explorer.Current();
    throw std::runtime_error("Top face missing");
}
// The edge of `shape` whose middle lies at `at`; for a circle, the one whose centre does.
TopoDS_Shape edgeAt(const TopoDS_Shape& shape, const gp_Pnt& at) {
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const gp_Pnt middle =
            curve.GetType() == GeomAbs_Circle
                ? curve.Circle().Location()
                : curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (middle.Distance(at) < 1e-9)
            return explorer.Current();
    }
    throw std::runtime_error("Edge missing");
}

void points() {
    // The reference plate 40 x 20 x 10: corner to corner, corner to the top face.
    const auto corner = vertex(0, 0, 0);
    const auto far = vertex(40, 20, 10);
    const auto diagonal = kernel::measureBetween(corner, far);
    require(near(diagonal.distance, std::sqrt(1600.0 + 400.0 + 100.0)) && diagonal.delta &&
                near((*diagonal.delta)[0], 40) && near((*diagonal.delta)[2], 10),
            "Corner to corner: the diagonal and its components");
    const auto plate = box(gp_Pnt(0, 0, 0), 40, 20, 10);
    const auto top = topFace(plate);
    const auto bottomFront = edgeAt(plate, gp_Pnt(20, 0, 0));
    require(near(kernel::measureBetween(corner, top).distance, 10), "Corner to the top face: 10");
    const auto middle = kernel::edgePoint(bottomFront, kernel::EdgePoint::middle);
    require(!middle.IsNull() && near(pointOf(middle)[0], 20) && near(pointOf(middle)[1], 0),
            "The edge middle lies at x 20");
    require(near(kernel::measureBetween(middle, far).distance, std::sqrt(400.0 + 400.0 + 100.0)),
            "Edge middle to the far corner");
    require(kernel::edgePoint(bottomFront, kernel::EdgePoint::center).IsNull(),
            "A straight edge has no centre");
    // A cylinder r 5 on (30, 10): its top circle's centre at (30, 10, 8).
    const auto cylinder =
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(30, 10, 0), gp::DZ()), 5, 8).Shape();
    const auto circle = edgeAt(cylinder, gp_Pnt(30, 10, 8));
    const auto center = kernel::edgePoint(circle, kernel::EdgePoint::center);
    require(!center.IsNull() && near(pointOf(center)[0], 30) && near(pointOf(center)[2], 8),
            "The circle's centre");
    require(near(kernel::measureBetween(corner, center).distance, std::sqrt(900.0 + 100.0 + 64.0)),
            "Corner to the circle centre");
}
} // namespace

int main() {
    try {
        interference();
        points();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS interference and point measures\n";
    return 0;
}
