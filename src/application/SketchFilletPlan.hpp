#pragma once

#include <voelund/application/SketchCurveEdit.hpp>

// S11: the geometry of a fillet between two lines. The corner is where the lines (extended)
// meet. Each line keeps the part away from the corner: the far end when the corner lies at or
// beyond an end, the selected end when the lines cross. The arc of the given radius touches
// both kept parts. Anything that cannot be filleted throws SketchEditRefusal naming the lines.
namespace voelund::application::detail {

struct SketchFilletLine final {
    document::SketchElementId id;
    document::SketchLine line;            // its new geometry, ending on the arc
    document::SketchPointPosition moved;  // the stored end that now touches the arc
    bool movedFromCorner;                 // that end was the corner point
};

struct SketchFilletPlan final {
    SketchFilletLine first, second;
    document::SketchArc arc;
    bool arcStartOnFirst; // the arc starts where the first line ends on it
    document::SketchPoint corner;
};

[[nodiscard]] SketchFilletPlan planFillet(document::SketchElementId firstId,
                                          const document::SketchLine& first,
                                          document::SketchPointPosition firstSelected,
                                          document::SketchElementId secondId,
                                          const document::SketchLine& second,
                                          document::SketchPointPosition secondSelected,
                                          double radius);

} // namespace voelund::application::detail
