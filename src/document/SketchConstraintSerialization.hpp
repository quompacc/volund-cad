#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <sstream>

namespace voelund::document {

const char* sketchConstraintKindName(SketchConstraintKind kind) noexcept;
SketchConstraintKind readSketchConstraintKind(std::istringstream& line);

} // namespace voelund::document
