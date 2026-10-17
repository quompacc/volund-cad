#pragma once

#include <voelund/application/SketchCurveEdit.hpp>

// S11: the geometry of a fillet between two lines. The corner is where the lines (extended)
// meet. Each line keeps the part away from the corner: the far end when the corner lies at or
// beyond an end, the selected end when the lines cross. The arc of the given radius touches
// both kept parts. Anything that cannot be filleted throws SketchEditRefusal naming the lines.
// S19: a chamfer cuts the corner with a line between the points `size` from it on each line.
namespace voelund::application::detail {

enum class SketchCornerKind { fillet, chamfer };

struct SketchFilletLine final {
    document::SketchElementId id;
    document::SketchLine line;           // its new geometry, ending on the arc
    document::SketchPointPosition moved; // the stored end that now touches the arc
    bool movedFromCorner;                // that end was the corner point
};

struct SketchFilletPlan final {
    SketchFilletLine first, second;
    document::SketchArc arc;      // fillet
    bool arcStartOnFirst;         // the arc starts where the first line ends on it
    document::SketchLine chamfer; // from the first line's touch to the second's
    document::SketchPoint corner;
};

[[nodiscard]] SketchFilletPlan
planCorner(SketchCornerKind kind, document::SketchElementId firstId,
           const document::SketchLine& first, document::SketchPointPosition firstSelected,
           document::SketchElementId secondId, const document::SketchLine& second,
           document::SketchPointPosition secondSelected, double size);

} // namespace voelund::application::detail
