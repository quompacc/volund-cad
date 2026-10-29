// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

// The rules every sketch element's geometry follows, one function per kind (N14c split them
// out of SketchGeometry.cpp): within the model range first, then finite and not degenerate.
namespace voelund::document::detail {

// Throws std::invalid_argument naming the broken rule.
void validateShape(const SketchGeometry& geometry);

} // namespace voelund::document::detail
