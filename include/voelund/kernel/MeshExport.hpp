// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/StepExchange.hpp>

#include <TopoDS_Shape.hxx>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// N03 (user decision 2026-10-22): solids as triangle meshes for 3D printing, in millimetres.
// Several solids go into one file and stay separate: STL holds all triangles, 3MF one object
// per solid with its name. Nothing here touches a document.
namespace voelund::kernel {

enum class MeshFormat { stl, threeMf };
enum class MeshResolution { coarse, medium, fine };

// Largest distance of the mesh from the surface (mm) and largest angle between neighbouring
// triangles of a curved face (radians).
struct MeshTolerance final {
    double chordal;
    double angle;
};
[[nodiscard]] MeshTolerance meshTolerance(MeshResolution resolution) noexcept;

// A closed mesh with outward triangles; faces share their vertices along edges.
struct Mesh final {
    std::string name; // UTF-8
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};
// Throws std::invalid_argument for a shape that is no solid or cannot be meshed.
[[nodiscard]] Mesh meshSolid(const TopoDS_Shape& solid, MeshResolution resolution,
                             std::string name = {});

// The bytes of the files: binary STL, and 3MF as an uncompressed ZIP package.
[[nodiscard]] std::string stlBytes(std::span<const Mesh> meshes);
[[nodiscard]] std::string threeMfBytes(std::span<const Mesh> meshes);

enum class MeshExportError {
    nothingToWrite, // no solids given
    notASolid,      // a shape that is no valid solid, or one that cannot be meshed
    writeFailed     // the file could not be written
};
struct MeshExport final {
    std::optional<MeshExportError> error{};
    std::string message{};
    std::size_t triangles{0};
};
[[nodiscard]] MeshExport writeMesh(const std::filesystem::path& path,
                                   std::span<const StepSolid> solids, MeshFormat format,
                                   MeshResolution resolution);

} // namespace voelund::kernel
