#include "SketchConstraintSerialization.hpp"

namespace voelund::document {

const char* sketchConstraintKindName(SketchConstraintKind kind) noexcept {
    switch (kind) {
    case SketchConstraintKind::horizontal:
        return "HORIZONTAL";
    case SketchConstraintKind::vertical:
        return "VERTICAL";
    case SketchConstraintKind::distance:
        return "DISTANCE";
    case SketchConstraintKind::radius:
        return "RADIUS";
    case SketchConstraintKind::coincident:
        return "COINCIDENT";
    case SketchConstraintKind::parallel:
        return "PARALLEL";
    case SketchConstraintKind::perpendicular:
        return "PERPENDICULAR";
    case SketchConstraintKind::coordinateX:
        return "COORDINATE_X";
    case SketchConstraintKind::coordinateY:
        return "COORDINATE_Y";
    case SketchConstraintKind::pointOnObject:
        return "POINT_ON_OBJECT";
    case SketchConstraintKind::equal:
        return "EQUAL";
    case SketchConstraintKind::tangent:
        return "TANGENT";
    case SketchConstraintKind::symmetric:
        return "SYMMETRIC";
    case SketchConstraintKind::pointDistance:
        return "POINT_DISTANCE";
    case SketchConstraintKind::angle:
        return "ANGLE";
    }
    return "HORIZONTAL";
}

} // namespace voelund::document
