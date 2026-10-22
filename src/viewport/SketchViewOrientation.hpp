// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <V3d_View.hxx>

namespace voelund::viewport {

void orientSketchView(const PlanarFaceSelection& support, const occ::handle<V3d_View>& view);

} // namespace voelund::viewport
