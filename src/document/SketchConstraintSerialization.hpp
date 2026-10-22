// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <sstream>

namespace voelund::document {

const char* sketchConstraintKindName(SketchConstraintKind kind) noexcept;
SketchConstraintKind readSketchConstraintKind(std::istringstream& line);

} // namespace voelund::document
