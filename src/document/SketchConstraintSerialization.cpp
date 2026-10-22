// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchConstraintSerialization.hpp"

#include <stdexcept>
#include <string>

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
    case SketchConstraintKind::pointDistanceX:
        return "POINT_DISTANCE_X";
    case SketchConstraintKind::pointDistanceY:
        return "POINT_DISTANCE_Y";
    case SketchConstraintKind::midpoint:
        return "MIDPOINT";
    case SketchConstraintKind::perpendicularBisector:
        return "PERPENDICULAR_BISECTOR";
    case SketchConstraintKind::concentric:
        return "CONCENTRIC";
    case SketchConstraintKind::diameter:
        return "DIAMETER";
    case SketchConstraintKind::collinear:
        return "COLLINEAR";
    case SketchConstraintKind::horizontalPoints:
        return "HORIZONTAL_POINTS";
    case SketchConstraintKind::verticalPoints:
        return "VERTICAL_POINTS";
    case SketchConstraintKind::fixed:
        return "FIXED";
    case SketchConstraintKind::pointLineDistance:
        return "POINT_LINE_DISTANCE";
    }
    return "HORIZONTAL";
}

SketchConstraintKind readSketchConstraintKind(std::istringstream& line) {
    std::string value;
    if (!(line >> value))
        throw std::runtime_error("Invalid Völund document: missing sketch constraint kind");
    if (value == "HORIZONTAL")
        return SketchConstraintKind::horizontal;
    if (value == "VERTICAL")
        return SketchConstraintKind::vertical;
    if (value == "DISTANCE")
        return SketchConstraintKind::distance;
    if (value == "RADIUS")
        return SketchConstraintKind::radius;
    if (value == "COINCIDENT")
        return SketchConstraintKind::coincident;
    if (value == "PARALLEL")
        return SketchConstraintKind::parallel;
    if (value == "PERPENDICULAR")
        return SketchConstraintKind::perpendicular;
    if (value == "COORDINATE_X")
        return SketchConstraintKind::coordinateX;
    if (value == "COORDINATE_Y")
        return SketchConstraintKind::coordinateY;
    if (value == "POINT_ON_OBJECT")
        return SketchConstraintKind::pointOnObject;
    if (value == "EQUAL")
        return SketchConstraintKind::equal;
    if (value == "TANGENT")
        return SketchConstraintKind::tangent;
    if (value == "SYMMETRIC")
        return SketchConstraintKind::symmetric;
    if (value == "POINT_DISTANCE")
        return SketchConstraintKind::pointDistance;
    if (value == "ANGLE")
        return SketchConstraintKind::angle;
    if (value == "POINT_DISTANCE_X")
        return SketchConstraintKind::pointDistanceX;
    if (value == "POINT_DISTANCE_Y")
        return SketchConstraintKind::pointDistanceY;
    if (value == "MIDPOINT")
        return SketchConstraintKind::midpoint;
    if (value == "PERPENDICULAR_BISECTOR")
        return SketchConstraintKind::perpendicularBisector;
    if (value == "CONCENTRIC")
        return SketchConstraintKind::concentric;
    if (value == "DIAMETER")
        return SketchConstraintKind::diameter;
    if (value == "COLLINEAR")
        return SketchConstraintKind::collinear;
    if (value == "HORIZONTAL_POINTS")
        return SketchConstraintKind::horizontalPoints;
    if (value == "VERTICAL_POINTS")
        return SketchConstraintKind::verticalPoints;
    if (value == "FIXED")
        return SketchConstraintKind::fixed;
    if (value == "POINT_LINE_DISTANCE")
        return SketchConstraintKind::pointLineDistance;
    throw std::runtime_error("Invalid Völund document: unknown sketch constraint kind");
}

} // namespace voelund::document
