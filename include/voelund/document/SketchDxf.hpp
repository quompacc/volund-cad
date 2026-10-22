#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <string>

namespace voelund::document {

// N04 (user decision 2026-10-22): a sketch as a DXF drawing (AutoCAD 2000 entities) in mm, in
// the sketch's own plane coordinates. Lines and rectangles become LINE, circles CIRCLE, arcs ARC
// (counter-clockwise), ellipses ELLIPSE, B-splines SPLINE (cubic, clamped, as in the sketch) and
// points POINT. Geometry is on layer "0", construction geometry on layer "Konstruktion".
[[nodiscard]] std::string sketchDxf(const SketchElements& elements);

} // namespace voelund::document
