// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/PlaneNormal.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include "OcctGuard.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Geom2d_Curve.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace voelund::kernel {
namespace {
constexpr auto operation = KernelOperation::evaluation;

std::optional<BoundingBox> boxOf(const Bnd_Box& box) {
    if (box.IsVoid())
        return std::nullopt;
    BoundingBox result{};
    box.Get(result.minimum[0], result.minimum[1], result.minimum[2], result.maximum[0],
            result.maximum[1], result.maximum[2]);
    return result;
}
} // namespace

gp_Pnt BoundingBox::centre() const noexcept {
    return gp_Pnt((minimum[0] + maximum[0]) / 2, (minimum[1] + maximum[1]) / 2,
                  (minimum[2] + maximum[2]) / 2);
}

double BoundingBox::diagonal() const noexcept {
    return std::hypot(maximum[0] - minimum[0], maximum[1] - minimum[1], maximum[2] - minimum[2]);
}

std::optional<BoundingBox> boundingBox(const TopoDS_Shape& shape) {
    return detail::guarded(operation, [&] {
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        return boxOf(box);
    });
}

gp_Pnt boundingBoxCentre(const TopoDS_Shape& shape) {
    const auto box = boundingBox(shape);
    if (!box)
        throw KernelError(KernelOperation::measure, "The shape has no extent");
    return box->centre();
}

std::optional<BoundingBox> tightBoundingBox(const TopoDS_Shape& shape) {
    return detail::guarded(operation, [&] {
        Bnd_Box box;
        BRepBndLib::AddOptimal(shape, box, false, false);
        return boxOf(box);
    });
}

FacePoint classifyOnFace(const TopoDS_Face& face, const gp_Pnt& point, double tolerance) {
    const auto state = detail::guarded(
        operation, [&] { return BRepClass_FaceClassifier(face, point, tolerance).State(); });
    switch (state) {
    case TopAbs_IN:
        return FacePoint::inside;
    case TopAbs_ON:
        return FacePoint::on;
    case TopAbs_OUT:
        return FacePoint::outside;
    default:
        return FacePoint::unknown;
    }
}

std::optional<gp_Pln> facePlane(const TopoDS_Shape& face) {
    return detail::guarded(operation, [&]() -> std::optional<gp_Pln> {
        const BRepAdaptor_Surface surface(TopoDS::Face(face));
        if (surface.GetType() != GeomAbs_Plane)
            return std::nullopt;
        return surface.Plane();
    });
}

std::optional<gp_Dir> planarFaceNormal(const TopoDS_Shape& face) {
    return detail::guarded(operation, [&]() -> std::optional<gp_Dir> {
        const BRepAdaptor_Surface surface(TopoDS::Face(face));
        if (surface.GetType() != GeomAbs_Plane)
            return std::nullopt;
        return planeNormal(surface.Plane(), face.Orientation());
    });
}

TopoDS_Shape transformedCopy(const TopoDS_Shape& shape, const gp_Trsf& transform) {
    return detail::guarded(
        operation, [&] { return BRepBuilderAPI_Transform(shape, transform, true).Shape(); });
}

TopoDS_Shape collect(std::span<const TopoDS_Shape> shapes) {
    return detail::guarded(operation, [&] {
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (const auto& shape : shapes)
            builder.Add(compound, shape);
        return compound;
    });
}

namespace {
// The outward normal of `face` where the edge's middle lies on it.
std::optional<gp_Vec> normalAtEdge(const TopoDS_Face& face, const TopoDS_Edge& edge) {
    double first = 0.0, last = 0.0;
    const Handle(Geom2d_Curve) onFace = BRep_Tool::CurveOnSurface(edge, face, first, last);
    if (onFace.IsNull())
        return std::nullopt;
    const gp_Pnt2d uv = onFace->Value((first + last) / 2);
    const BRepAdaptor_Surface surface(face);
    BRepLProp_SLProps properties(surface, uv.X(), uv.Y(), 1, 1e-7);
    if (!properties.IsNormalDefined())
        return std::nullopt;
    gp_Vec normal(properties.Normal());
    if (face.Orientation() == TopAbs_REVERSED)
        normal.Reverse();
    return normal;
}
} // namespace

std::optional<gp_Dir> intoEdgeCorner(const TopoDS_Shape& body, const TopoDS_Shape& edge) {
    return detail::guarded(operation, [&]() -> std::optional<gp_Dir> {
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                   TopTools_ShapeMapHasher>
            faces;
        TopExp::MapShapesAndAncestors(body, TopAbs_EDGE, TopAbs_FACE, faces);
        if (edge.ShapeType() != TopAbs_EDGE || !faces.Contains(edge))
            return std::nullopt;
        gp_Vec sum(0, 0, 0);
        int count = 0;
        std::vector<TopoDS_Shape> seen;
        for (const auto& face : faces.FindFromKey(edge)) {
            if (std::ranges::any_of(seen, [&](const auto& known) { return known.IsSame(face); }))
                continue;
            seen.push_back(face);
            const auto normal = normalAtEdge(TopoDS::Face(face), TopoDS::Edge(edge));
            if (!normal)
                return std::nullopt;
            sum += *normal;
            ++count;
        }
        if (count != 2 || sum.Magnitude() < 1e-9)
            return std::nullopt;
        return gp_Dir(sum.Reversed());
    });
}

} // namespace voelund::kernel
