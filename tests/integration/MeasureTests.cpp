#include "CadSessionTestSupport.hpp"

#include <voelund/kernel/Measurement.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M12 (A-MEASURE-01, user decision 2026-10-15): measuring a box 20 x 10 x 10 gives edges of 20
// and 10, 90 degrees between two edges and between two faces, 10 between opposite faces; a
// ring of radii 20 and 10 extruded 8 has a ring face of 300 pi mm², circular edges of radius 20
// and 10 and a volume of 2400 pi mm³. Measuring changes neither the document nor its state.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

TopoDS_Shape find(const TopoDS_Shape& body, TopAbs_ShapeEnum kind,
                  const std::function<bool(const TopoDS_Shape&)>& wanted) {
    for (TopExp_Explorer explorer(body, kind); explorer.More(); explorer.Next())
        if (wanted(explorer.Current()))
            return explorer.Current();
    throw std::runtime_error("Shape to measure missing");
}
// A straight edge along `axis` (0 = X, 1 = Y, 2 = Z).
std::function<bool(const TopoDS_Shape&)> along(int axis) {
    return [axis](const TopoDS_Shape& shape) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        if (curve.GetType() != GeomAbs_Line)
            return false;
        const auto direction = curve.Line().Direction();
        const double parts[] = {direction.X(), direction.Y(), direction.Z()};
        return std::abs(std::abs(parts[axis]) - 1) < 1e-9;
    };
}
// A planar face whose normal runs along `axis`, at `level` on that axis.
std::function<bool(const TopoDS_Shape&)> facing(int axis, double level) {
    return [axis, level](const TopoDS_Shape& shape) {
        const BRepAdaptor_Surface surface(TopoDS::Face(shape));
        if (surface.GetType() != GeomAbs_Plane)
            return false;
        const auto plane = surface.Plane();
        const auto normal = plane.Axis().Direction();
        const double parts[] = {normal.X(), normal.Y(), normal.Z()};
        const double at[] = {plane.Location().X(), plane.Location().Y(), plane.Location().Z()};
        return std::abs(std::abs(parts[axis]) - 1) < 1e-9 && near(at[axis], level);
    };
}

void measureBox() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 10, 10}).feature();
    const auto revision = session.sceneStatus().document;
    const bool modified = session.isModified();
    const auto shape = *session.featureShape(box);
    const auto x = find(shape, TopAbs_EDGE, along(0));
    const auto y = find(shape, TopAbs_EDGE, along(1));
    require(near(*kernel::measureShape(x).length, 20) && near(*kernel::measureShape(y).length, 10),
            "Box edges of 20 and 10");
    const auto edges = kernel::measureBetween(x, y);
    require(edges.angle && near(*edges.angle, pi / 2), "90 degrees between two box edges");
    const auto bottom = find(shape, TopAbs_FACE, facing(2, 0));
    const auto top = find(shape, TopAbs_FACE, facing(2, 10));
    const auto side = find(shape, TopAbs_FACE, facing(0, 0));
    const auto faces = kernel::measureBetween(bottom, top);
    require(near(faces.distance, 10) && faces.angle && near(*faces.angle, 0),
            "Opposite faces 10 apart and parallel");
    require(near(*kernel::measureBetween(bottom, side).angle, pi / 2),
            "90 degrees between neighbouring faces");
    require(
        near(*kernel::measureBetween(x, bottom).angle, 0) &&
            near(*kernel::measureBetween(x, side).angle, pi / 2) &&
            near(*kernel::measureBetween(find(shape, TopAbs_EDGE, along(2)), bottom).angle, pi / 2),
        "An edge against a face: along it 0, across it 90 degrees");
    require(near(*kernel::measureShape(top).area, 200) &&
                near(*kernel::measureShape(shape).volume, 2000),
            "Face area and body volume");
    require(session.sceneStatus().document == revision && session.isModified() == modified,
            "Measuring changes nothing");
}

void measureRing() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters ring{document::datumSketchPlane(document::DatumPlane::xy), {}};
    ring.datum = document::DatumPlane::xy;
    ring.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{0, 0}, 20}});
    ring.elements.values.push_back(
        {document::SketchElementId{2}, document::SketchCircle{{0, 0}, 10}});
    ring.elements.nextId = 3;
    require(session.createDatumSketchExtrusion(ring, {}, 8).status() == CommandStatus::committed,
            "Ring extruded 8 (empty regions: the outer one with its hole)");
    const auto body = session.validBodies().front().shape;
    const auto top = find(body, TopAbs_FACE, facing(2, 8));
    require(near(*kernel::measureShape(top).area, 300 * pi), "Ring face of 300 pi mm²");
    require(near(*kernel::measureShape(body).volume, 2400 * pi), "Volume of 2400 pi mm³");
    const auto outer = find(body, TopAbs_EDGE, [](const TopoDS_Shape& shape) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        return curve.GetType() == GeomAbs_Circle && near(curve.Circle().Radius(), 20);
    });
    require(near(*kernel::measureShape(outer).radius, 20) &&
                near(*kernel::measureShape(outer).length, 40 * pi),
            "Outer circular edge: radius 20, length 40 pi");
    const auto inner = find(body, TopAbs_FACE, [](const TopoDS_Shape& shape) {
        const BRepAdaptor_Surface surface(TopoDS::Face(shape));
        return surface.GetType() == GeomAbs_Cylinder && near(surface.Cylinder().Radius(), 10);
    });
    require(near(*kernel::measureShape(inner).radius, 10), "Hole face radius 10");
}

} // namespace

int main() {
    try {
        measureBox();
        measureRing();
        std::cout << "PASS M12 measure (A-MEASURE-01): box edges and angles, face distance, area, "
                     "volume, ring 300 pi, volume 2400 pi, radii, no document change\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
