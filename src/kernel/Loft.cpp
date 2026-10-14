#include <voelund/kernel/Loft.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <optional>
#include <string>

namespace voelund::kernel {

struct Loft::Impl final {
    std::optional<BRepOffsetAPI_ThruSections> maker;
    TopoDS_Shape shape;
};

Loft::Loft(const std::vector<TopoDS_Shape>& sections, bool ruled)
    : impl_(std::make_unique<Impl>()) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::loft, diagnostic);
    };
    if (sections.size() < 2)
        fail("a loft needs at least two sections");
    for (const auto& section : sections)
        if (section.IsNull() || section.ShapeType() != TopAbs_WIRE)
            fail("each section is one closed wire");
    try {
        impl_->maker.emplace(true, ruled);
        for (const auto& section : sections)
            impl_->maker->AddWire(TopoDS::Wire(section));
        impl_->maker->CheckCompatibility(false);
        impl_->maker->Build();
        if (!impl_->maker->IsDone())
            fail("the loft failed");
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
        fail("the loft produced no solid");
    if (!BRepCheck_Analyzer(impl_->shape).IsValid())
        fail("the loft produced an invalid solid");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(impl_->shape, properties);
    if (!(std::abs(properties.Mass()) > 0.0))
        fail("the loft produced no volume");
}

Loft::~Loft() = default;
Loft::Loft(Loft&&) noexcept = default;
Loft& Loft::operator=(Loft&&) noexcept = default;

const TopoDS_Shape& Loft::shape() const noexcept { return impl_->shape; }

TopoDS_Shape Loft::startFace() const { return impl_->maker->FirstShape(); }

TopoDS_Shape Loft::endFace() const { return impl_->maker->LastShape(); }

TopoDS_Shape Loft::generated(const TopoDS_Shape& firstSectionEdge) const {
    try {
        return impl_->maker->GeneratedFace(firstSectionEdge);
    } catch (const Standard_Failure&) {
        return {}; // a side the history does not name stays unnamed
    }
}

} // namespace voelund::kernel
