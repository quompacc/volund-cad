// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <span>
#include <vector>
#include <voelund/document/SketchGeometry.hpp>
namespace voelund::features {
// Derived, non-persistent graph. Geometry is never snapped or rewritten.
struct ContourPiece final {
    document::SketchElementId element;
    unsigned part; // Rectangle sides 0..3; all other curves 0.
    document::SketchGeometry geometry;
    document::SketchPoint start;
    document::SketchPoint end;
    bool periodic;
};
struct OrientedContourPiece final {
    std::size_t piece;
    bool reversed;
};
struct ContourLoop final {
    std::vector<OrientedContourPiece> boundary;
};
enum class ContourIssue { openEnd, branch, overlap, ambiguousConnection, degenerate };
struct ContourDiagnostic final {
    ContourIssue issue;
    std::vector<document::SketchElementId> elements;
};
struct SketchContourGraph final {
    std::vector<ContourPiece> pieces;
    std::vector<ContourLoop> loops;
    std::vector<ContourDiagnostic> diagnostics;
};
// Requires document-valid elements. Intersections/containment belong to R08.
[[nodiscard]] SketchContourGraph
buildSketchContourGraph(std::span<const document::SketchElement> elements);
} // namespace voelund::features
