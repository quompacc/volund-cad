// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/PlaneNormal.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

#include <cmath>

namespace voelund::kernel {
namespace {
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
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    return boxOf(box);
}

gp_Pnt boundingBoxCentre(const TopoDS_Shape& shape) {
    const auto box = boundingBox(shape);
    if (!box)
        throw KernelError(KernelOperation::measure, "The shape has no extent");
    return box->centre();
}

std::optional<BoundingBox> tightBoundingBox(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    return boxOf(box);
}

FacePoint classifyOnFace(const TopoDS_Face& face, const gp_Pnt& point, double tolerance) {
    switch (BRepClass_FaceClassifier(face, point, tolerance).State()) {
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
    const BRepAdaptor_Surface surface(TopoDS::Face(face));
    if (surface.GetType() != GeomAbs_Plane)
        return std::nullopt;
    return surface.Plane();
}

std::optional<gp_Dir> planarFaceNormal(const TopoDS_Shape& face) {
    const BRepAdaptor_Surface surface(TopoDS::Face(face));
    if (surface.GetType() != GeomAbs_Plane)
        return std::nullopt;
    return planeNormal(surface.Plane(), face.Orientation());
}

TopoDS_Shape transformedCopy(const TopoDS_Shape& shape, const gp_Trsf& transform) {
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

TopoDS_Shape collect(std::span<const TopoDS_Shape> shapes) {
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& shape : shapes)
        builder.Add(compound, shape);
    return compound;
}

} // namespace voelund::kernel
