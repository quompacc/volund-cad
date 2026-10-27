// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/EdgeQueries.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

// W07 (as Fusion): a double click on an edge takes the edges joined to it without a kink. On a
// box with one rounded upright edge, a top edge runs over the round into the next top edge and
// stops at the sharp corners; a plain box edge stands alone.
namespace {
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
TopoDS_Shape edgeWhere(const TopoDS_Shape& shape,
                       const std::function<bool(const gp_Pnt&)>& wanted) {
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        if (wanted(curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2)))
            return explorer.Current();
    }
    throw std::runtime_error("edge missing");
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
} // namespace

int main() {
    try {
        using voelund::kernel::tangentChain;
        const TopoDS_Shape box = BRepPrimAPI_MakeBox(20, 20, 10).Shape();
        const auto plain =
            edgeWhere(box, [](const gp_Pnt& p) { return near(p.Y(), 0) && near(p.Z(), 10); });
        require(tangentChain(box, plain).size() == 1, "A plain box edge stands alone");
        BRepFilletAPI_MakeFillet fillet(box);
        fillet.Add(3, TopoDS::Edge(edgeWhere(
                          box, [](const gp_Pnt& p) { return near(p.X(), 20) && near(p.Y(), 0); })));
        const TopoDS_Shape rounded = fillet.Shape();
        const auto top = edgeWhere(rounded, [](const gp_Pnt& p) {
            return near(p.Y(), 0) && near(p.Z(), 10) && p.X() < 15;
        });
        const auto chain = tangentChain(rounded, top);
        require(chain.size() == 3,
                "Over the round into the next top edge: " + std::to_string(chain.size()));
        std::cout << "PASS tangent chain: plain edge alone, over a round, stops at corners\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
