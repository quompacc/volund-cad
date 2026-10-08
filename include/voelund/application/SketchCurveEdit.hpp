#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <optional>
#include <vector>

namespace voelund::application {

// S10: the curve edits of the sketch editor and the curve types each one supports.
//
// | Edit    | Line | Arc | Circle            | Ellipse, rectangle |
// |---------|------|-----|-------------------|--------------------|
// | trim    | yes  | yes | yes (becomes arc) | refused            |
// | extend  | yes  | yes | refused (closed)  | refused            |
// | split   | yes  | yes | refused (no end)  | refused            |
//
// Cutting edges are all other lines, arcs, circles and rectangle edges, construction geometry
// included; ellipses do not cut. Trim removes the piece between the cuts next to the picked
// point (a piece in the middle leaves two elements), extend moves the end nearest the pick to
// the next edge in its direction, split divides at the picked point.
enum class SketchCurveEdit { trim, extend, split };

struct SketchCurveEditResult final {
    // The second piece of a trim in the middle or of a split; it keeps the element's end.
    std::optional<document::SketchElementId> created;
    // Relations that bound the moved end or the old length; they no longer hold and are gone.
    std::vector<document::SketchConstraint> removed;
    // Relations that tie the pieces together and the new ends to their cutting edges.
    std::vector<document::SketchConstraintId> added;
};

} // namespace voelund::application
