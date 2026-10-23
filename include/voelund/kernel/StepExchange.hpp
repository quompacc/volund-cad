// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// M17: STEP files at the edge of the kernel. Solids come in and go out in millimetres with the
// names of their STEP products; the file's own unit is reported. Nothing here touches a
// document: a failed read leaves everything as it was.
namespace voelund::kernel {

struct StepSolid final {
    TopoDS_Shape solid;
    std::string name;                     // UTF-8; empty when the file names none
    std::optional<std::uint32_t> color{}; // N06: 0xRRGGBB of its faces; none when unset
};

enum class StepImportError {
    unreadable, // no STEP file, or one OCCT cannot parse or transfer
    noSolids    // read, but nothing in it is a valid solid
};
struct StepImport final {
    std::vector<StepSolid> solids;
    std::string sourceUnit{};     // the file's length unit as it names it ("MILLIMETRE", "INCH")
    std::size_t skippedShapes{0}; // shells, faces and curves that are no solid
    std::size_t invalidSolids{0}; // solids still invalid after shape healing
    std::optional<StepImportError> error{};
    std::string message{};
};
[[nodiscard]] StepImport readStep(const std::filesystem::path& path);

enum class StepLengthUnit { millimetre, inch };
enum class StepExportError {
    nothingToWrite, // no solids given
    notASolid,      // a shape that is no valid solid
    writeFailed     // transfer or file write failed
};
struct StepExport final {
    std::optional<StepExportError> error{};
    std::string message{};
};
// Writes the solids as products of their names; `unit` is the unit of the file (the shapes are
// in mm either way).
[[nodiscard]] StepExport writeStep(const std::filesystem::path& path,
                                   std::span<const StepSolid> solids,
                                   StepLengthUnit unit = StepLengthUnit::millimetre);

// N04: writes the solids as IGES in mm (BRep mode: manifold solids, entity 186) with their
// names; refused and reported like a STEP export.
[[nodiscard]] StepExport writeIges(const std::filesystem::path& path,
                                   std::span<const StepSolid> solids);

// M17: an imported solid embedded in a document, as OCCT binary BRep. readShapeBytes returns a
// null shape for data that is no valid solid.
[[nodiscard]] std::string writeShapeBytes(const TopoDS_Shape& shape);
[[nodiscard]] TopoDS_Shape readShapeBytes(std::string_view bytes);

} // namespace voelund::kernel
