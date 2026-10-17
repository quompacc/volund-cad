#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/Prism.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <optional>

namespace voelund::kernel {

struct Prism::Impl final {
    std::optional<BRepPrimAPI_MakePrism> maker;
    TopoDS_Shape shape;
};

Prism::Prism(const TopoDS_Shape& profile, double directionX, double directionY, double directionZ,
             double length)
    : impl_(std::make_unique<Impl>()) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::makePrism, diagnostic);
    };
    if (profile.IsNull() || !TopExp_Explorer(profile, TopAbs_FACE).More())
        fail("the profile has no face");
    const double norm =
        std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    if (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-9)
        fail("the direction is not a unit vector");
    if (!std::isfinite(length) || !modelLength(length))
        fail("the length is outside the model range");
    try {
        impl_->maker.emplace(profile, gp_Vec(directionX, directionY, directionZ) * length);
        if (!impl_->maker->IsDone())
            fail("the sweep failed");
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
        fail("the sweep produced no solid");
    if (!BRepCheck_Analyzer(impl_->shape).IsValid())
        fail("the sweep produced an invalid solid");
}

Prism::~Prism() = default;
Prism::Prism(Prism&&) noexcept = default;
Prism& Prism::operator=(Prism&&) noexcept = default;

const TopoDS_Shape& Prism::shape() const noexcept { return impl_->shape; }

TopoDS_Shape Prism::startOf(const TopoDS_Shape& profileShape) const {
    return impl_->maker->FirstShape(profileShape);
}

TopoDS_Shape Prism::endOf(const TopoDS_Shape& profileShape) const {
    return impl_->maker->LastShape(profileShape);
}

std::vector<TopoDS_Shape> Prism::generated(const TopoDS_Shape& profileShape) const {
    std::vector<TopoDS_Shape> result;
    for (const auto& item : impl_->maker->Generated(profileShape))
        result.push_back(item);
    return result;
}

} // namespace voelund::kernel
