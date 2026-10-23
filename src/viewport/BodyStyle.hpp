// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_Shape.hxx>
#include <Quantity_Color.hxx>

#include <cstdint>

namespace voelund::viewport {

// N06: a body colour 0xRRGGBB as an OCCT colour.
[[nodiscard]] Quantity_Color rgbColor(std::uint32_t color);

// N06 (user decision 2026-10-23): edges and display mode of a body presentation after the
// display style. Wireframe and hidden edges draw the edges in `edge`; hidden edges are dashed
// when the view computes hidden lines (V3d_View::SetComputedMode). Call after SetColor.
void styleBody(const occ::handle<AIS_Shape>& shape, DisplayStyle style, const Quantity_Color& edge);

// The display mode the style draws bodies in.
[[nodiscard]] int bodyDisplayMode(DisplayStyle style) noexcept;

} // namespace voelund::viewport
