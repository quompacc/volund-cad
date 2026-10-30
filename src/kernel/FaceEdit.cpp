// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FaceReplacement.hpp"
#include "OperationHistoryDetail.hpp"
#include "ShapeValidity.hpp"
#include <voelund/kernel/FaceEdit.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <Precision.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::kernel {
namespace {

constexpr auto operation = KernelOperation::evaluation;

using AncestorMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

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

bool sameVolume(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    GProp_GProps one;
    GProp_GProps other;
    BRepGProp::VolumeProperties(first, one);
    BRepGProp::VolumeProperties(second, other);
    return std::abs(one.Mass() - other.Mass()) <= 1e-9 * std::max(1.0, std::abs(other.Mass()));
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

gp_Dir outwardNormal(const TopoDS_Face& face, gp_Pnt& point) {
    BRepGProp_Face properties(face);
    double uMin = 0;
    double uMax = 0;
    double vMin = 0;
    double vMax = 0;
    properties.Bounds(uMin, uMax, vMin, vMax);
    gp_Vec normal;
    properties.Normal((uMin + uMax) / 2, (vMin + vMax) / 2, point, normal);
    return gp_Dir(normal);
}

bool sameSurface(const BRepAdaptor_Surface& a, const BRepAdaptor_Surface& b) {
    if (a.GetType() != b.GetType())
        return false;
    if (a.GetType() == GeomAbs_Plane)
        return a.Plane().Position().IsCoplanar(b.Plane().Position(), 1e-7, 1e-7);
    if (a.GetType() == GeomAbs_Cylinder)
        return std::abs(a.Cylinder().Radius() - b.Cylinder().Radius()) < 1e-7 &&
               a.Cylinder().Axis().IsCoaxial(b.Cylinder().Axis(), 1e-7, 1e-7);
    return false;
}

using FaceList = std::vector<TopoDS_Face>;
bool listed(const FaceList& faces, const TopoDS_Shape& face) {
    return std::ranges::any_of(faces, [&](const auto& each) { return each.IsSame(face); });
}

// Whether the two faces are tangent along their shared edge, checked at its middle.
bool tangentAlong(const TopoDS_Edge& edge, const TopoDS_Face& first, const TopoDS_Face& second) {
    double start = 0;
    double end = 0;
    const auto one = BRep_Tool::CurveOnSurface(edge, first, start, end);
    double start2 = 0;
    double end2 = 0;
    const auto two = BRep_Tool::CurveOnSurface(edge, second, start2, end2);
    if (one.IsNull() || two.IsNull())
        return false;
    const auto a = one->Value((start + end) / 2);
    const auto b = two->Value((start2 + end2) / 2);
    BRepGProp_Face propertiesA(first);
    BRepGProp_Face propertiesB(second);
    gp_Pnt point;
    gp_Vec normalA;
    gp_Vec normalB;
    propertiesA.Normal(a.X(), a.Y(), point, normalA);
    propertiesB.Normal(b.X(), b.Y(), point, normalB);
    if (normalA.Magnitude() < 1e-12 || normalB.Magnitude() < 1e-12)
        return false;
    return normalA.Angle(normalB) < 1e-5;
}

// The faces that have to go along: split parts of the same surface, and for a plane the rounded
// faces tangent to it (a fillet rolls along).
FaceList facesMovedWith(const TopoDS_Shape& input, std::span<const TopoDS_Shape> chosen,
                        bool followRounds) {
    AncestorMap edgeFaces;
    TopExp::MapShapesAndAncestors(input, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    FaceList faces;
    for (const auto& face : chosen)
        faces.push_back(TopoDS::Face(face));
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const TopoDS_Face face = faces[index];
        const BRepAdaptor_Surface own(face);
        for (TopExp_Explorer edges(face, TopAbs_EDGE); edges.More(); edges.Next()) {
            const auto& edge = TopoDS::Edge(edges.Current());
            for (const auto& shape : edgeFaces.FindFromKey(edge)) {
                const auto& other = TopoDS::Face(shape);
                if (listed(faces, other))
                    continue;
                const BRepAdaptor_Surface surface(other);
                const bool round = followRounds && surface.GetType() != GeomAbs_Plane &&
                                   tangentAlong(edge, face, other);
                if (sameSurface(own, surface) || round)
                    faces.push_back(other);
            }
        }
    }
    return faces;
}

occ::handle<Geom_Surface> movedCylinder(const TopoDS_Face& face, double distance) {
    const BRepAdaptor_Surface surface(face);
    auto cylinder = surface.Cylinder();
    gp_Pnt point;
    const gp_Dir normal = outwardNormal(face, point);
    const gp_Vec toAxis(point, cylinder.Axis().Location());
    const gp_Vec along(cylinder.Axis().Direction());
    const bool wall = (toAxis - along * toAxis.Dot(along)).Dot(gp_Vec(normal)) > 0;
    const double radius = cylinder.Radius() + (wall ? -distance : distance);
    if (radius <= Precision::Confusion())
        throw KernelError(operation, "the faces cannot be moved this far");
    cylinder.SetRadius(radius);
    return new Geom_CylindricalSurface(cylinder);
}

// I05: planes and cylinders moved along their normals with the topology kept. No value when a
// chosen face is of another kind or the faces around cannot follow.
std::optional<ShapeOperationResult> offsetByReplacement(const TopoDS_Shape& input,
                                                        std::span<const TopoDS_Shape> chosen,
                                                        double distance) {
    std::vector<detail::FaceReplacement> replacements;
    int group = 0;
    for (const auto& shape : chosen) {
        const auto& face = TopoDS::Face(shape);
        if (std::ranges::any_of(replacements,
                                [&](const auto& item) { return item.face.IsSame(face); }))
            continue;
        const BRepAdaptor_Surface surface(face);
        ++group;
        if (surface.GetType() == GeomAbs_Plane) {
            gp_Pnt point;
            gp_Trsf move;
            move.SetTranslation(gp_Vec(outwardNormal(face, point)) * distance);
            const TopoDS_Shape one[] = {face};
            for (const auto& each : facesMovedWith(input, one, true))
                replacements.push_back({each,
                                        occ::handle<Geom_Surface>::DownCast(
                                            BRep_Tool::Surface(each)->Transformed(move)),
                                        move, group});
        } else if (surface.GetType() == GeomAbs_Cylinder) {
            const TopoDS_Shape one[] = {face};
            for (const auto& each : facesMovedWith(input, one, false))
                replacements.push_back({each, movedCylinder(each, distance), std::nullopt, group});
        } else {
            return std::nullopt;
        }
    }
    try {
        auto result = detail::replaceFaces(input, replacements);
        requireSolid(result.shape, "the faces cannot be moved this far");
        return result;
    } catch (const KernelError&) {
        return std::nullopt;
    } catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

} // namespace

ShapeOperationResult offsetFacesWithHistory(const TopoDS_Shape& input,
                                            std::span<const TopoDS_Shape> faces, double distance) {
    requireInput(input, faces);
    if (!(std::abs(distance) > Precision::Confusion()))
        throw KernelError(operation, "the faces move by a distance other than 0");
    if (auto replaced = offsetByReplacement(input, faces, distance))
        return std::move(*replaced);
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
        // I01: on real parts OCCT sometimes reports success with the input unchanged.
        if (defeaturing.Shape().IsSame(input) || sameVolume(defeaturing.Shape(), input))
            throw KernelError(operation, "the faces around cannot close the gap");
        ShapeOperationResult result{defeaturing.Shape(), {}, {}};
        detail::appendHistory(result, defeaturing, true, input, HistoryOperand::first);
        return result;
    });
}

} // namespace voelund::kernel
