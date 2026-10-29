// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OperationHistoryDetail.hpp"
#include "ShapeValidity.hpp"
#include <voelund/kernel/FaceEdit.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

namespace voelund::kernel {
namespace {

constexpr auto operation = KernelOperation::evaluation;

// OCCT's offset maker with a value on each chosen face and none on the others; its history is
// the maker's own.
class FaceOffset final : public BRepOffsetAPI_MakeOffsetShape {
  public:
    void perform(const TopoDS_Shape& input, std::span<const TopoDS_Shape> faces, double distance) {
        myOffsetShape.Initialize(input, 0.0, Precision::Confusion(), BRepOffset_Skin, true, false,
                                 GeomAbs_Intersection);
        for (const auto& face : faces)
            myOffsetShape.SetOffsetOnFace(TopoDS::Face(face), distance);
        myOffsetShape.MakeOffsetShape();
        myLastUsedAlgo = OffsetAlgo_JOIN;
        if (!myOffsetShape.IsDone())
            return;
        myShape = myOffsetShape.Shape();
        Done();
    }
};

void requireSolid(const TopoDS_Shape& result, const char* what) {
    if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More() ||
        !detail::isValidShape(result))
        throw KernelError(operation, what);
    GProp_GProps properties;
    BRepGProp::VolumeProperties(result, properties);
    if (!(properties.Mass() > 0.0))
        throw KernelError(operation, what);
}

void requireInput(const TopoDS_Shape& input, std::span<const TopoDS_Shape> faces) {
    if (input.IsNull() || !TopExp_Explorer(input, TopAbs_SOLID).More() || faces.empty())
        throw KernelError(operation, "a face edit needs a solid and its faces");
}

template <class Build> ShapeOperationResult guardedEdit(Build&& build) {
    try {
        return build();
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        throw KernelError(operation, occtFailureDiagnostic(error));
    }
}

} // namespace

ShapeOperationResult offsetFacesWithHistory(const TopoDS_Shape& input,
                                            std::span<const TopoDS_Shape> faces, double distance) {
    requireInput(input, faces);
    if (!(std::abs(distance) > Precision::Confusion()))
        throw KernelError(operation, "the faces move by a distance other than 0");
    return guardedEdit([&] {
        FaceOffset offset;
        offset.perform(input, faces, distance);
        if (!offset.IsDone())
            throw KernelError(operation, "the faces cannot be moved this far");
        requireSolid(offset.Shape(), "the faces cannot be moved this far");
        ShapeOperationResult result{offset.Shape(), {}, {}};
        detail::appendHistory(result, offset, true, input, HistoryOperand::first);
        return result;
    });
}

ShapeOperationResult removeFacesWithHistory(const TopoDS_Shape& input,
                                            std::span<const TopoDS_Shape> faces) {
    requireInput(input, faces);
    return guardedEdit([&] {
        BRepAlgoAPI_Defeaturing defeaturing;
        defeaturing.SetShape(input);
        for (const auto& face : faces)
            defeaturing.AddFaceToRemove(face);
        defeaturing.SetToFillHistory(true);
        defeaturing.Build();
        // A feature it cannot take out is a warning, and the input comes back unchanged.
        if (!defeaturing.IsDone() || defeaturing.HasErrors() || defeaturing.HasWarnings())
            throw KernelError(operation, "the faces around cannot close the gap");
        requireSolid(defeaturing.Shape(), "the faces around cannot close the gap");
        ShapeOperationResult result{defeaturing.Shape(), {}, {}};
        detail::appendHistory(result, defeaturing, true, input, HistoryOperand::first);
        return result;
    });
}

} // namespace voelund::kernel
