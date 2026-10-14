#include <voelund/kernel/Sweep.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <optional>
#include <string>

namespace voelund::kernel {
namespace {

gp_Pnt centre(const TopoDS_Shape& face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.CentreOfMass();
}

// The faces of a cap compound in explorer order.
std::vector<TopoDS_Shape> faces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Shape> result;
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next())
        result.push_back(it.Current());
    return result;
}

} // namespace

struct Sweep::Impl final {
    std::optional<BRepOffsetAPI_MakePipe> maker;
    TopoDS_Shape shape;
    std::vector<TopoDS_Shape> profileFaces, firstFaces, lastFaces;
    bool closed{false};
};

Sweep::Sweep(const TopoDS_Shape& profile, const TopoDS_Shape& path)
    : impl_(std::make_unique<Impl>()) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::makePipe, diagnostic);
    };
    if (profile.IsNull() || !TopExp_Explorer(profile, TopAbs_FACE).More())
        fail("the profile has no face");
    if (path.IsNull() || path.ShapeType() != TopAbs_WIRE)
        fail("the path is not one connected wire");
    impl_->closed = BRep_Tool::IsClosed(path);
    try {
        impl_->maker.emplace(TopoDS::Wire(path), profile, GeomFill_IsCorrectedFrenet);
        impl_->maker->Build();
        if (!impl_->maker->IsDone())
            fail("the sweep failed");
        impl_->shape = impl_->maker->Shape();
        impl_->profileFaces = faces(profile);
        impl_->firstFaces = faces(impl_->maker->FirstShape());
        impl_->lastFaces = faces(impl_->maker->LastShape());
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
        fail("the sweep produced no solid");
    if (!BRepCheck_Analyzer(impl_->shape).IsValid())
        fail("the sweep produced an invalid solid; the path may bend too tightly");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(impl_->shape, properties);
    if (!(properties.Mass() > 0.0))
        fail("the sweep produced no volume");
}

Sweep::~Sweep() = default;
Sweep::Sweep(Sweep&&) noexcept = default;
Sweep& Sweep::operator=(Sweep&&) noexcept = default;

const TopoDS_Shape& Sweep::shape() const noexcept { return impl_->shape; }

bool Sweep::closed() const noexcept { return impl_->closed; }

// The first cap of a profile face is the one where the face stands (the profile stays in
// place); its last cap has the same place among the last faces.
TopoDS_Shape Sweep::startOf(const TopoDS_Shape& profileFace) const {
    if (impl_->closed || impl_->firstFaces.size() != impl_->profileFaces.size())
        return {};
    const gp_Pnt at = centre(profileFace);
    for (const auto& face : impl_->firstFaces)
        if (centre(face).Distance(at) < 1e-6)
            return face;
    return {};
}

TopoDS_Shape Sweep::endOf(const TopoDS_Shape& profileFace) const {
    const auto start = startOf(profileFace);
    if (start.IsNull() || impl_->lastFaces.size() != impl_->firstFaces.size())
        return {};
    for (std::size_t index = 0; index < impl_->firstFaces.size(); ++index)
        if (impl_->firstFaces[index].IsSame(start))
            return impl_->lastFaces[index];
    return {};
}

std::vector<TopoDS_Shape> Sweep::generated(const TopoDS_Shape& profileEdge) const {
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
