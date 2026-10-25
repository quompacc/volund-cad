// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/ShapeQueries.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

// User report 2026-10-25: the fillet's arrow points into the corner of the picked edge, also
// where a face is curved (the rim of a cylinder), not only between planar faces.
namespace {
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

// The first edge of `shape` whose middle the predicate accepts.
TopoDS_Shape edgeWhere(const TopoDS_Shape& shape,
                       const std::function<bool(const gp_Pnt&)>& wanted) {
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        if (wanted(curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2)))
            return explorer.Current();
    }
    throw std::runtime_error("edge missing");
}
} // namespace

int main() {
    try {
        using voelund::kernel::intoEdgeCorner;
        const auto box = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
        const auto corner =
            intoEdgeCorner(box, edgeWhere(box, [](const gp_Pnt& p) {
                               return std::abs(p.X() - 10) < 1e-9 && std::abs(p.Y() - 10) < 1e-9;
                           }));
        require(corner && corner->X() < -0.7 && corner->Y() < -0.7 && std::abs(corner->Z()) < 1e-9,
                "A box edge: into the corner between its faces");
        const auto cylinder = BRepPrimAPI_MakeCylinder(5, 10).Shape();
        const auto rim = intoEdgeCorner(cylinder, edgeWhere(cylinder, [](const gp_Pnt& p) {
                                            return std::abs(p.Z() - 10) < 1e-9;
                                        }));
        require(rim && rim->Z() < -0.5, "The rim of a cylinder: down into the corner");
        std::cout << "PASS edge corner direction: planar and curved faces\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
