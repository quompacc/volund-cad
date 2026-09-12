#include "SketchConstraintLabel.hpp"

#include <voelund/document/SketchGeometry.hpp>

#include <iomanip>
#include <sstream>

namespace voelund::desktop {

std::string sketchConstraintLabel(const features::SketchConstraintAnnotation& annotation) {
    using document::SketchConstraintKind;
    if (annotation.kind == SketchConstraintKind::horizontal)
        return "[H]";
    if (annotation.kind == SketchConstraintKind::vertical)
        return "[V]";
    if (annotation.kind == SketchConstraintKind::coincident)
        return "[C]";
    if (annotation.kind == SketchConstraintKind::parallel)
        return "[||]";
    if (annotation.kind == SketchConstraintKind::perpendicular)
        return "[90]";
    if (annotation.kind == SketchConstraintKind::collinear)
        return "[CL]";
    if (annotation.kind == SketchConstraintKind::horizontalPoints)
        return "[HP]";
    if (annotation.kind == SketchConstraintKind::verticalPoints)
        return "[VP]";
    if (annotation.kind == SketchConstraintKind::fixed)
        return "[F]";
    if (annotation.kind == SketchConstraintKind::coordinateX)
        return "[X]";
    if (annotation.kind == SketchConstraintKind::coordinateY)
        return "[Y]";
    if (annotation.kind == SketchConstraintKind::pointOnObject)
        return "[O]";
    if (annotation.kind == SketchConstraintKind::equal)
        return "[=]";
    if (annotation.kind == SketchConstraintKind::tangent)
        return "[T]";
    if (annotation.kind == SketchConstraintKind::symmetric)
        return "[S]";
    if (annotation.kind == SketchConstraintKind::midpoint)
        return "[M]";
    if (annotation.kind == SketchConstraintKind::perpendicularBisector)
        return "[PB]";
    if (annotation.kind == SketchConstraintKind::concentric)
        return "[CC]";
    std::ostringstream label;
    if (annotation.kind == SketchConstraintKind::angle) {
        label << "A " << std::fixed << std::setprecision(3) << annotation.value << "°";
        return label.str();
    }
    if (annotation.kind == SketchConstraintKind::pointDistance)
        label << "D ";
    else if (annotation.kind == SketchConstraintKind::pointLineDistance)
        label << "PL ";
    else if (annotation.kind == SketchConstraintKind::pointDistanceX)
        label << "DX ";
    else if (annotation.kind == SketchConstraintKind::pointDistanceY)
        label << "DY ";
    else if (annotation.kind == SketchConstraintKind::diameter)
        label << "Ø ";
    else
        label << (annotation.kind == SketchConstraintKind::radius ? "R " : "L ");
    label << std::fixed << std::setprecision(3) << annotation.value << " mm";
    return label.str();
}

} // namespace voelund::desktop
