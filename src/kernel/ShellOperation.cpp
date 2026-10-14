#include "OperationHistoryDetail.hpp"

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <GProp_GProps.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Shape.hxx>

// M22: a shell ("Wandstärke") of a solid; ShapeOperations' other members are in
// ShapeOperations.cpp.
namespace voelund::kernel {

ShapeOperationResult ShapeOperations::shellWithHistory(const TopoDS_Shape& input,
                                                       std::span<const TopoDS_Shape> openFaces,
                                                       double thickness, bool outward) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::shell, diagnostic, BooleanFailure::shellFailed);
    };
    if (input.IsNull() || !TopExp_Explorer(input, TopAbs_SOLID).More())
        throw KernelError(KernelOperation::shell, "a shell needs a solid");
    if (openFaces.empty())
        throw KernelError(KernelOperation::shell, "a shell opens at least one face");
    if (!(thickness > 0.0))
        throw KernelError(KernelOperation::shell, "the thickness must be positive");
    try {
        NCollection_List<TopoDS_Shape> faces;
        for (const auto& face : openFaces)
            faces.Append(face);
        BRepOffsetAPI_MakeThickSolid maker;
        // Inward walls keep the outer faces where they are (negative offset).
        maker.MakeThickSolidByJoin(input, faces, outward ? thickness : -thickness, 1e-6,
                                   BRepOffset_Skin, false, false, GeomAbs_Intersection);
        if (!maker.IsDone() || maker.Shape().IsNull() ||
            !TopExp_Explorer(maker.Shape(), TopAbs_SOLID).More() ||
            !BRepCheck_Analyzer(maker.Shape()).IsValid())
            fail("the body cannot take walls of this thickness");
        GProp_GProps properties;
        BRepGProp::VolumeProperties(maker.Shape(), properties);
        if (!(properties.Mass() > 0.0))
            fail("the walls leave no volume");
        ShapeOperationResult result{maker.Shape(), {}, {}};
        detail::appendHistory(result, maker, true, input, HistoryOperand::first);
        return result;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        fail(occtFailureDiagnostic(error));
    }
    return {};
}

} // namespace voelund::kernel
