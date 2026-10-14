#include "OperationHistoryDetail.hpp"

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <GProp_GProps.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <NCollection_List.hxx>
#include <numbers>
#include <string>

// M25: a draft ("Formschräge") of body faces; ShapeOperations' other members are in
// ShapeOperations.cpp.
namespace voelund::kernel {
namespace {

// The draft reports a tilted face only through its modifier (ModifiedShape), not Modified().
struct DraftHistory final {
    BRepOffsetAPI_DraftAngle& maker;
    NCollection_List<TopoDS_Shape> modified;
    const NCollection_List<TopoDS_Shape>& Modified(const TopoDS_Shape& source) {
        modified.Clear();
        for (const auto& shape : maker.Modified(source))
            modified.Append(shape);
        if (modified.IsEmpty()) {
            try {
                const auto shape = maker.ModifiedShape(source);
                if (!shape.IsNull() && !shape.IsSame(source))
                    modified.Append(shape);
            } catch (const Standard_Failure&) {
            }
        }
        return modified;
    }
    decltype(auto) Generated(const TopoDS_Shape& source) { return maker.Generated(source); }
    bool IsDeleted(const TopoDS_Shape& source) { return maker.IsDeleted(source); }
};

} // namespace

DraftFaceError::DraftFaceError(std::size_t face)
    : std::runtime_error("draft: face " + std::to_string(face + 1) + " cannot be drafted"),
      face_(face) {}

ShapeOperationResult ShapeOperations::draftWithHistory(const TopoDS_Shape& input,
                                                       std::span<const TopoDS_Shape> faces,
                                                       const std::array<double, 3>& neutralOrigin,
                                                       const std::array<double, 3>& pullDirection,
                                                       double angle) {
    const auto fail = [](const std::string& diagnostic) {
        throw KernelError(KernelOperation::draft, diagnostic);
    };
    if (input.IsNull() || !TopExp_Explorer(input, TopAbs_SOLID).More())
        fail("a draft needs a solid");
    if (faces.empty())
        fail("a draft tilts at least one face");
    if (!std::isfinite(angle) || std::abs(angle) <= 1e-9 ||
        std::abs(angle) >= std::numbers::pi / 2)
        fail("the angle lies between 0 and 90 degrees");
    const double norm = std::hypot(pullDirection[0], pullDirection[1], pullDirection[2]);
    if (!std::isfinite(norm) || norm < 1e-12)
        fail("the pull direction has no length");
    try {
        const gp_Dir pull(pullDirection[0], pullDirection[1], pullDirection[2]);
        const gp_Pln neutral(gp_Pnt(neutralOrigin[0], neutralOrigin[1], neutralOrigin[2]), pull);
        BRepOffsetAPI_DraftAngle maker(input);
        for (std::size_t index = 0; index < faces.size(); ++index) {
            if (faces[index].IsNull() || faces[index].ShapeType() != TopAbs_FACE)
                throw DraftFaceError(index);
            maker.Add(TopoDS::Face(faces[index]), pull, angle, neutral);
            if (!maker.AddDone())
                throw DraftFaceError(index);
        }
        maker.Build();
        if (!maker.IsDone() || maker.Shape().IsNull() ||
            !TopExp_Explorer(maker.Shape(), TopAbs_SOLID).More() ||
            !BRepCheck_Analyzer(maker.Shape()).IsValid())
            fail("the faces cannot be tilted together");
        GProp_GProps properties;
        BRepGProp::VolumeProperties(maker.Shape(), properties);
        if (!(properties.Mass() > 0.0))
            fail("the draft leaves no volume");
        ShapeOperationResult result{maker.Shape(), {}, {}};
        DraftHistory history{maker, {}};
        detail::appendHistory(result, history, true, input, HistoryOperand::first);
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
