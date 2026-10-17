#include <voelund/kernel/Helix.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax3.hxx>
#include <gp_Lin2d.hxx>

#include <cmath>
#include <numbers>
#include <optional>
#include <string>

namespace voelund::kernel {

struct Helix::Impl final {
    std::optional<BRepOffsetAPI_MakePipeShell> maker;
    TopoDS_Shape shape;
};

Helix::Helix(const TopoDS_Shape& profileFace, const std::array<double, 3>& axisOrigin,
             const std::array<double, 3>& axisDirection, double pitch, double angle,
             bool leftHanded)
    : impl_(std::make_unique<Impl>()) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::helix, diagnostic);
    };
    if (profileFace.IsNull() || profileFace.ShapeType() != TopAbs_FACE)
        fail("the profile is not one face");
    int wires = 0;
    for (TopExp_Explorer it(profileFace, TopAbs_WIRE); it.More(); it.Next())
        ++wires;
    if (wires != 1)
        fail("the profile has holes");
    const double norm = std::hypot(axisDirection[0], axisDirection[1], axisDirection[2]);
    if (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-9)
        fail("the axis is not a unit vector");
    if (!std::isfinite(pitch) || pitch <= 1e-9 || !std::isfinite(angle) || angle <= 1e-9)
        fail("pitch and turn must be positive");
    // The helix starts at the profile's centre, on the cylinder about the axis through it.
    GProp_GProps area;
    BRepGProp::SurfaceProperties(profileFace, area);
    const gp_Pnt centre = area.CentreOfMass();
    const gp_Pnt origin(axisOrigin[0], axisOrigin[1], axisOrigin[2]);
    const gp_Dir axis(axisDirection[0], axisDirection[1], axisDirection[2]);
    const gp_Vec toCentre(origin, centre);
    const double along = toCentre.Dot(gp_Vec(axis));
    const gp_Vec radial = toCentre - gp_Vec(axis) * along;
    const double radius = radial.Magnitude();
    if (radius < 1e-6)
        fail("the profile lies on the axis");
    try {
        const gp_Ax3 frame(origin.Translated(gp_Vec(axis) * along), axis, gp_Dir(radial));
        const Handle(Geom_CylindricalSurface) cylinder = new Geom_CylindricalSurface(frame, radius);
        // In the cylinder's (angle, height) parameters a helix is a line.
        const gp_Dir2d slope(leftHanded ? -2.0 * std::numbers::pi : 2.0 * std::numbers::pi, pitch);
        const Handle(Geom2d_Line) line = new Geom2d_Line(gp_Lin2d(gp_Pnt2d(0, 0), slope));
        const double length =
            angle / (2.0 * std::numbers::pi) * std::hypot(2.0 * std::numbers::pi, pitch);
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line, cylinder, 0.0, length);
        BRepLib::BuildCurves3d(edge);
        const TopoDS_Wire spine = BRepBuilderAPI_MakeWire(edge);
        impl_->maker.emplace(spine);
        impl_->maker->SetMode(axis); // binormal: the profile keeps its attitude to the axis
        impl_->maker->Add(BRepTools::OuterWire(TopoDS::Face(profileFace)));
        impl_->maker->Build();
        if (!impl_->maker->IsDone() || !impl_->maker->MakeSolid())
            fail("the helix sweep failed");
        impl_->shape = impl_->maker->Shape();
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        fail(occtFailureDiagnostic(failure));
    }
    if (impl_->shape.IsNull() || !TopExp_Explorer(impl_->shape, TopAbs_SOLID).More())
        fail("the helix produced no solid");
    if (!BRepCheck_Analyzer(impl_->shape).IsValid())
        fail("the helix produced an invalid solid; the turns may overlap");
    GProp_GProps volume;
    BRepGProp::VolumeProperties(impl_->shape, volume);
    if (!(std::abs(volume.Mass()) > 0.0))
        fail("the helix produced no volume");
}

Helix::~Helix() = default;
Helix::Helix(Helix&&) noexcept = default;
Helix& Helix::operator=(Helix&&) noexcept = default;

const TopoDS_Shape& Helix::shape() const noexcept { return impl_->shape; }

TopoDS_Shape Helix::startFace() const { return impl_->maker->FirstShape(); }

TopoDS_Shape Helix::endFace() const { return impl_->maker->LastShape(); }

std::vector<TopoDS_Shape> Helix::generated(const TopoDS_Shape& profileEdge) const {
    std::vector<TopoDS_Shape> result;
    try {
        for (const auto& item : impl_->maker->Generated(profileEdge))
            if (item.ShapeType() == TopAbs_FACE)
                result.push_back(item);
    } catch (const Standard_Failure&) {
        return {}; // a side the history does not name stays unnamed
    }
    return result;
}

} // namespace voelund::kernel
