// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/Prism.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>

#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// W16 (user request 2026-10-25, FreeCAD's "Schrägungswinkel"): a tapered prism widens with a
// positive angle and narrows with a negative one, keeps holes and round profiles, and names
// its caps and sides as the straight prism does.
namespace {
using voelund::kernel::Prism;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
double area(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape, properties);
    return properties.Mass();
}
TopoDS_Wire square(double half) {
    BRepBuilderAPI_MakePolygon polygon(gp_Pnt(-half, -half, 0), gp_Pnt(half, -half, 0),
                                       gp_Pnt(half, half, 0), gp_Pnt(-half, half, 0), true);
    return polygon.Wire();
}
// A frustum of square ends: h/3 (A1 + A2 + sqrt(A1 A2)).
double squareFrustum(double side, double top, double height) {
    return height / 3 * (side * side + top * top + side * top);
}
double radians(double degrees) { return degrees * std::numbers::pi / 180; }

void widensAndNarrows() {
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(square(5)).Face();
    const double grow = 10 * std::tan(radians(10));
    const Prism wider(face, gp_Vec(0, 0, 10), radians(10));
    require(std::abs(volume(wider.shape()) - squareFrustum(10, 10 + 2 * grow, 10)) < 1e-3,
            "Positive: wider at the end, " + std::to_string(volume(wider.shape())));
    const Prism narrower(face, gp_Vec(0, 0, 10), radians(-10));
    require(std::abs(volume(narrower.shape()) - squareFrustum(10, 10 - 2 * grow, 10)) < 1e-3,
            "Negative: narrower at the end");
    require(std::abs(area(narrower.startOf(face)) - 100) < 1e-6 &&
                std::abs(area(narrower.endOf(face)) - std::pow(10 - 2 * grow, 2)) < 1e-3,
            "Start and end caps are found");
    const TopoDS_Shape edge = TopExp_Explorer(face, TopAbs_EDGE).Current();
    require(narrower.generated(edge).size() == 1 && !narrower.startOf(edge).IsNull() &&
                !narrower.endOf(edge).IsNull(),
            "An edge names its side face and its cap edges");
}

void holesAndCircles() {
    BRepBuilderAPI_MakeFace withHole(square(10));
    withHole.Add(TopoDS::Wire(square(5).Reversed()));
    const double grow = 10 * std::tan(radians(5));
    const Prism ring(withHole.Face(), gp_Vec(0, 0, 10), radians(5));
    const double expected =
        squareFrustum(20, 20 + 2 * grow, 10) - squareFrustum(10, 10 - 2 * grow, 10);
    require(std::abs(volume(ring.shape()) - expected) < 1e-3,
            "The hole narrows while the outside widens: " + std::to_string(volume(ring.shape())));
    const gp_Circ circle(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 5);
    const TopoDS_Face disc =
        BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle))).Face();
    const Prism cone(disc, gp_Vec(0, 0, 10), radians(-15));
    const double top = 5 - 10 * std::tan(radians(15));
    const double frustum = std::numbers::pi * 10 / 3 * (25 + 5 * top + top * top);
    require(std::abs(volume(cone.shape()) - frustum) < 1e-2, "A circle becomes a cone frustum");
    const TopoDS_Shape rim = TopExp_Explorer(disc, TopAbs_EDGE).Current();
    require(cone.generated(rim).size() == 1, "The circle names its cone face");
}

void closes() {
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(square(5)).Face();
    try {
        const Prism closed(face, gp_Vec(0, 0, 10), radians(-40));
    } catch (const voelund::kernel::KernelError&) {
        return;
    }
    throw std::runtime_error("A taper that closes the profile is refused");
}
} // namespace

int main() {
    try {
        widensAndNarrows();
        holesAndCircles();
        closes();
        std::cout << "PASS tapered prism: wider, narrower, holes, circles, history\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
