// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "TaperedPrism.hpp"
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

namespace {
[[noreturn]] void fail(const std::string& diagnostic) {
    throw KernelError(KernelOperation::makePrism, diagnostic);
}

void checkLength(double length) {
    if (!std::isfinite(length) || !modelLength(length))
        fail("the length is outside the model range");
}

// The sweep vector of a unit direction and a length in the model range.
gp_Vec checkedSweep(double directionX, double directionY, double directionZ, double length) {
    const double norm =
        std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    if (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-9)
        fail("the direction is not a unit vector");
    checkLength(length);
    return gp_Vec(directionX, directionY, directionZ) * length;
}
} // namespace

struct Prism::Impl final {
    std::optional<BRepPrimAPI_MakePrism> maker;
    std::optional<TaperedPrism> tapered; // W16
    TopoDS_Shape shape;

    void sweep(const TopoDS_Shape& profile, const gp_Vec& along, double taper) {
        if (taper != 0.0) {
            shape = tapered.emplace(profile, along, taper).shape();
            return;
        }
        maker.emplace(profile, along);
        if (!maker->IsDone())
            fail("the sweep failed");
        shape = maker->Shape();
    }
};

Prism::Prism(const TopoDS_Shape& profile, double directionX, double directionY, double directionZ,
             double length)
    : Prism(profile, checkedSweep(directionX, directionY, directionZ, length), 0.0) {}

Prism::Prism(const TopoDS_Shape& profile, const gp_Vec& sweep, double taper)
    : impl_(std::make_unique<Impl>()) {
    if (profile.IsNull() || !TopExp_Explorer(profile, TopAbs_FACE).More())
        fail("the profile has no face");
    checkLength(sweep.Magnitude());
    if (!std::isfinite(taper) || std::abs(taper) >= maximumTaper)
        fail("the taper angle is outside the range");
    try {
        impl_->sweep(profile, sweep, taper);
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

bool Prism::tapered() const noexcept { return impl_->tapered.has_value(); }

TopoDS_Shape Prism::startOf(const TopoDS_Shape& profileShape) const {
    if (impl_->tapered)
        return impl_->tapered->onCap(profileShape, false);
    return impl_->maker->FirstShape(profileShape);
}

TopoDS_Shape Prism::endOf(const TopoDS_Shape& profileShape) const {
    if (impl_->tapered)
        return impl_->tapered->onCap(profileShape, true);
    return impl_->maker->LastShape(profileShape);
}

std::vector<TopoDS_Shape> Prism::generated(const TopoDS_Shape& profileShape) const {
    if (impl_->tapered)
        return impl_->tapered->sides(profileShape);
    std::vector<TopoDS_Shape> result;
    for (const auto& item : impl_->maker->Generated(profileShape))
        result.push_back(item);
    return result;
}

} // namespace voelund::kernel
