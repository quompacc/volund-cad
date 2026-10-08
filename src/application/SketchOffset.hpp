#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <span>
#include <vector>

// S09: exact offsets of sketch geometry. One element or a connected contour of lines and arcs
// is offset by a normal distance; line corners are re-intersected, tangent joints stay joined.
// Positive distances grow circles, arcs of a closed contour and closed contours outwards; an
// open contour moves to the left of its direction (the first selected element's direction).
// Anything that cannot be offset exactly throws std::invalid_argument with the reason.
namespace voelund::application::detail {

struct SketchOffsetJoint final {
    std::size_t first; // index into SketchOffsetResult::elements
    document::SketchPointPosition firstPoint;
    std::size_t second;
    document::SketchPointPosition secondPoint;
};

struct SketchOffsetResult final {
    std::vector<document::SketchGeometry> elements;
    std::vector<SketchOffsetJoint> joints; // where consecutive offset elements meet
};

[[nodiscard]] SketchOffsetResult offsetGeometry(std::span<const document::SketchGeometry> selected,
                                                double distance);

} // namespace voelund::application::detail
