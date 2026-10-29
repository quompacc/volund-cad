// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

// N15b (agent decision 2026-10-29): the drawing of an ASCII DXF file as sketch curves, in
// millimetres ($INSUNITS; none counts as millimetres). Read from ENTITIES: LINE, CIRCLE, ARC,
// LWPOLYLINE and POLYLINE (straight and bulged segments), POINT, ELLIPSE and non-rational SPLINE
// (degree 1 to 3, made cubic and clamped); the layer "Konstruktion" (as the export writes it)
// is construction geometry. Entities seen from below (extrusion 0 0 -1) are mirrored back where
// that is plain (lines, circles, arcs, polylines). Everything else (blocks, text, hatches,
// 3D entities) is skipped and counted by type.
namespace voelund::application::detail {

struct DxfCurve final {
    document::SketchGeometry geometry;
    bool construction;
};

struct DxfDrawing final {
    std::vector<DxfCurve> curves;
    std::map<std::string, std::size_t> skipped; // entity type -> how many
};

// Throws std::invalid_argument when the text is no readable ASCII DXF.
[[nodiscard]] DxfDrawing readDxfDrawing(std::string_view text);

} // namespace voelund::application::detail
