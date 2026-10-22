// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/Revolution.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Ax1.hxx>

#include <cmath>
#include <numbers>
#include <optional>

namespace voelund::kernel {

struct Revolution::Impl final {
    std::optional<BRepPrimAPI_MakeRevol> maker;
    TopoDS_Shape shape;
    bool full{false};
};

Revolution::Revolution(const TopoDS_Shape& profile, const std::array<double, 3>& axisOrigin,
                       const std::array<double, 3>& axisDirection, double angle)
    : impl_(std::make_unique<Impl>()) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::makeRevolution, diagnostic);
    };
    if (profile.IsNull() || !TopExp_Explorer(profile, TopAbs_FACE).More())
        fail("the profile has no face");
    const double norm = std::hypot(axisDirection[0], axisDirection[1], axisDirection[2]);
    if (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-9)
        fail("the axis is not a unit vector");
    if (!std::isfinite(angle) || angle <= 1e-9 || angle > 2.0 * std::numbers::pi + 1e-12)
        fail("the angle is outside (0, 360] degrees");
    impl_->full = angle >= 2.0 * std::numbers::pi - 1e-12;
    const gp_Ax1 axis(gp_Pnt(axisOrigin[0], axisOrigin[1], axisOrigin[2]),
                      gp_Dir(axisDirection[0], axisDirection[1], axisDirection[2]));
    try {
        if (impl_->full)
            impl_->maker.emplace(profile, axis);
        else
            impl_->maker.emplace(profile, axis, angle);
        if (!impl_->maker->IsDone())
            fail("the turn failed");
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
        fail("the turn produced no solid");
    if (!BRepCheck_Analyzer(impl_->shape).IsValid())
        fail("the turn produced an invalid solid; the axis may cross the profile");
    // A profile crossing the axis can still pass the check with a self-overlapping solid;
    // its volume then comes out non-positive.
    GProp_GProps properties;
    BRepGProp::VolumeProperties(impl_->shape, properties);
    if (!(properties.Mass() > 0.0))
        fail("the turn produced no volume; the axis may cross the profile");
}

Revolution::~Revolution() = default;
Revolution::Revolution(Revolution&&) noexcept = default;
Revolution& Revolution::operator=(Revolution&&) noexcept = default;

const TopoDS_Shape& Revolution::shape() const noexcept { return impl_->shape; }

bool Revolution::full() const noexcept { return impl_->full; }

TopoDS_Shape Revolution::startOf(const TopoDS_Shape& profileShape) const {
    return impl_->full ? TopoDS_Shape{} : impl_->maker->FirstShape(profileShape);
}

TopoDS_Shape Revolution::endOf(const TopoDS_Shape& profileShape) const {
    return impl_->full ? TopoDS_Shape{} : impl_->maker->LastShape(profileShape);
}

std::vector<TopoDS_Shape> Revolution::generated(const TopoDS_Shape& profileShape) const {
    std::vector<TopoDS_Shape> result;
    for (const auto& item : impl_->maker->Generated(profileShape))
        result.push_back(item);
    return result;
}

} // namespace voelund::kernel
