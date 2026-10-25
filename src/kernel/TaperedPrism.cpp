// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "TaperedPrism.hpp"
#include <voelund/kernel/KernelError.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_List.hxx>
#include <ShapeAnalysis_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <string>

namespace voelund::kernel {
namespace {

constexpr double placeTolerance = 1e-6;

[[noreturn]] void fail(const std::string& diagnostic) {
    throw KernelError(KernelOperation::makePrism, diagnostic);
}

bool onPlane(const TopoDS_Shape& shape, const gp_Pln& plane) {
    for (TopExp_Explorer vertex(shape, TopAbs_VERTEX); vertex.More(); vertex.Next())
        if (plane.Distance(BRep_Tool::Pnt(TopoDS::Vertex(vertex.Current()))) > placeTolerance)
            return false;
    return true;
}

gp_Pnt middle(const TopoDS_Shape& edge) {
    const BRepAdaptor_Curve curve(TopoDS::Edge(edge));
    return curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
}

// The point lies on the edge, within its ends.
bool liesOn(const gp_Pnt& point, const TopoDS_Shape& edge) {
    gp_Pnt projected;
    double parameter = 0.0;
    const BRepAdaptor_Curve curve(TopoDS::Edge(edge));
    return ShapeAnalysis_Curve().Project(curve, point, placeTolerance, projected, parameter) <
           placeTolerance;
}

// Two edges on one curve piece: each one's middle lies on the other.
bool sameRun(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    return liesOn(middle(first), second) && liesOn(middle(second), first);
}

bool contains(const TopoDS_Shape& shape, const TopoDS_Shape& part) {
    for (TopExp_Explorer explorer(shape, part.ShapeType()); explorer.More(); explorer.Next())
        if (explorer.Current().IsSame(part))
            return true;
    return false;
}

// The wire of the offset result that holds what the wire's edges became.
TopoDS_Wire offsetWire(BRepOffsetAPI_MakeOffset& offset, const TopoDS_Wire& wire) {
    for (TopExp_Explorer edge(wire, TopAbs_EDGE); edge.More(); edge.Next())
        for (const auto& made : offset.Generated(edge.Current()))
            for (TopExp_Explorer result(offset.Shape(), TopAbs_WIRE); result.More(); result.Next())
                if (contains(result.Current(), made))
                    return TopoDS::Wire(result.Current());
    fail("the taper closes the profile");
}

// A ruled loft from the wire to its offset copy, as a solid of positive volume.
TopoDS_Shape loft(const TopoDS_Wire& bottom, const TopoDS_Wire& top) {
    BRepOffsetAPI_ThruSections maker(true, true);
    maker.AddWire(bottom);
    maker.AddWire(top);
    maker.Build();
    if (!maker.IsDone())
        fail("the tapered sweep failed");
    TopoDS_Shape solid = maker.Shape();
    GProp_GProps properties;
    BRepGProp::VolumeProperties(solid, properties);
    if (properties.Mass() < 0.0)
        solid.Reverse();
    return solid;
}

// The face swept by `sweep` and leaned by `taper`: the loft of its outer wire less those of
// its holes.
TopoDS_Shape taperFace(const TopoDS_Face& face, const gp_Vec& sweep, double taper) {
    gp_Trsf move;
    move.SetTranslation(sweep);
    const TopoDS_Face top = TopoDS::Face(BRepBuilderAPI_Transform(face, move, true).Shape());
    BRepOffsetAPI_MakeOffset offset(top, GeomAbs_Intersection);
    offset.Perform(sweep.Magnitude() * std::tan(taper));
    if (!offset.IsDone())
        fail("the taper closes the profile");
    const TopoDS_Wire outer = BRepTools::OuterWire(face);
    TopoDS_Shape result;
    NCollection_List<TopoDS_Shape> holes;
    TopoDS_Iterator topWires(top);
    for (TopoDS_Iterator wires(face); wires.More(); wires.Next(), topWires.Next()) {
        const auto solid =
            loft(TopoDS::Wire(wires.Value()), offsetWire(offset, TopoDS::Wire(topWires.Value())));
        if (wires.Value().IsSame(outer))
            result = solid;
        else
            holes.Append(solid);
    }
    if (holes.IsEmpty())
        return result;
    BRepAlgoAPI_Cut cut;
    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(result);
    cut.SetArguments(arguments);
    cut.SetTools(holes);
    cut.Build();
    if (!cut.IsDone())
        fail("the holes of the tapered sweep could not be cut");
    return cut.Shape();
}

} // namespace

TaperedPrism::TaperedPrism(const TopoDS_Shape& profile, const gp_Vec& sweep, double taper) {
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (TopExp_Explorer face(profile, TopAbs_FACE); face.More(); face.Next()) {
        const TopoDS_Face& profileFace = TopoDS::Face(face.Current());
        parts_.push_back({profileFace, taperFace(profileFace, sweep, taper)});
        builder.Add(compound, parts_.back().solid);
    }
    const gp_Pln plane = BRepAdaptor_Surface(parts_.front().profile).Plane();
    start_ = plane;
    end_ = plane.Translated(sweep);
    shape_ = compound;
}

TopoDS_Shape TaperedPrism::onCap(const TopoDS_Shape& profileShape, bool end) const {
    for (const Part& part : parts_) {
        if (profileShape.ShapeType() == TopAbs_EDGE && contains(part.profile, profileShape))
            return capEdge(part, profileShape, end);
        if (!part.profile.IsSame(profileShape))
            continue;
        for (TopExp_Explorer face(part.solid, TopAbs_FACE); face.More(); face.Next())
            if (onPlane(face.Current(), end ? end_ : start_))
                return face.Current();
    }
    return {};
}

TopoDS_Shape TaperedPrism::capEdge(const Part& part, const TopoDS_Shape& edge, bool end) const {
    const auto side = sides(edge);
    for (const auto& face : side) {
        if (!contains(part.solid, face))
            continue;
        for (TopExp_Explorer cap(face, TopAbs_EDGE); cap.More(); cap.Next())
            if (onPlane(cap.Current(), end ? end_ : start_))
                return cap.Current();
    }
    return {};
}

std::vector<TopoDS_Shape> TaperedPrism::sides(const TopoDS_Shape& profileShape) const {
    std::vector<TopoDS_Shape> result;
    if (profileShape.ShapeType() != TopAbs_EDGE)
        return result;
    for (const Part& part : parts_) {
        if (!contains(part.profile, profileShape))
            continue;
        for (TopExp_Explorer face(part.solid, TopAbs_FACE); face.More(); face.Next()) {
            if (onPlane(face.Current(), start_) || onPlane(face.Current(), end_))
                continue;
            for (TopExp_Explorer edge(face.Current(), TopAbs_EDGE); edge.More(); edge.Next())
                if (onPlane(edge.Current(), start_) && sameRun(edge.Current(), profileShape)) {
                    result.push_back(face.Current());
                    break;
                }
        }
    }
    return result;
}

} // namespace voelund::kernel
