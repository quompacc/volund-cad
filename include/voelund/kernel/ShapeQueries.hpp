// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <array>
#include <optional>
#include <span>

// K-GRENZE (2026-10-22): small queries and builders on shapes for the layers above the kernel,
// which use no OCCT algorithms themselves. TopoDS shapes and gp values travel as plain data.
namespace voelund::kernel {

struct BoundingBox final {
    std::array<double, 3> minimum;
    std::array<double, 3> maximum;
    [[nodiscard]] gp_Pnt centre() const noexcept;
    [[nodiscard]] double diagonal() const noexcept;
};
// The axis-aligned box around the shape (with OCCT's usual gap); none for an empty shape.
[[nodiscard]] std::optional<BoundingBox> boundingBox(const TopoDS_Shape& shape);
// The middle of the box around the shape; KernelError for an empty shape.
[[nodiscard]] gp_Pnt boundingBoxCentre(const TopoDS_Shape& shape);
// The tight box (exact on the geometry, no triangulation, no gap).
[[nodiscard]] std::optional<BoundingBox> tightBoundingBox(const TopoDS_Shape& shape);

enum class FacePoint { inside, outside, on, unknown };
// Where `point` lies against `face` within `tolerance`.
[[nodiscard]] FacePoint classifyOnFace(const TopoDS_Face& face, const gp_Pnt& point,
                                       double tolerance);

// The plane of a planar face (its own frame, not turned by the face's orientation); none for a
// face that is not planar.
[[nodiscard]] std::optional<gp_Pln> facePlane(const TopoDS_Shape& face);

// The outward normal of a planar face as it lies in its shape (its orientation counts); none for
// a face that is not planar.
[[nodiscard]] std::optional<gp_Dir> planarFaceNormal(const TopoDS_Shape& face);

// A copy of the shape moved by the transformation (geometry copied).
[[nodiscard]] TopoDS_Shape transformedCopy(const TopoDS_Shape& shape, const gp_Trsf& transform);

// The shapes in one compound; an empty list gives an empty compound.
[[nodiscard]] TopoDS_Shape collect(std::span<const TopoDS_Shape> shapes);

} // namespace voelund::kernel
