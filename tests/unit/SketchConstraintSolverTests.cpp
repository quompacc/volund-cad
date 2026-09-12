#include <voelund/features/SketchConstraintSolver.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace {

constexpr double tolerance = 1.0e-7;

bool close(double first, double second) { return std::abs(first - second) <= tolerance; }

voelund::document::SketchPlane plane() { return {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0}; }

voelund::document::SketchGeometryReference lineRef(std::uint64_t id) {
    return {voelund::document::SketchElementId{id}, voelund::document::SketchPointPosition::start};
}

bool adapterSolvesVoeIundGeometryAndReportsDegreesOfFreedom() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {3.0, 4.0}}},
          {SketchElementId{2}, SketchCircle{{6.0, 2.0}, 1.0}}},
         3},
        {{{SketchConstraintId{10}, SketchConstraintKind::distance, lineRef(1), std::nullopt, 10.0},
          {SketchConstraintId{11}, SketchConstraintKind::horizontal, lineRef(1), std::nullopt, 0.0},
          {SketchConstraintId{12},
           SketchConstraintKind::radius,
           {SketchElementId{2}, SketchPointPosition::center},
           std::nullopt,
           2.5}},
         13}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& line = std::get<SketchLine>(sketch.elements.values[0].geometry);
    const auto& circle = std::get<SketchCircle>(sketch.elements.values[1].geometry);
    return result.status == voelund::features::SketchSolveStatus::success &&
           result.degreesOfFreedom == 4 && result.conflictingConstraints.empty() &&
           close(line.start.y, line.end.y) &&
           close(std::hypot(line.end.x - line.start.x, line.end.y - line.start.y), 10.0) &&
           close(circle.radius, 2.5);
}

bool adapterMapsPlaneGcsConflictsBackToStableIds() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {3.0, 4.0}}}}, 2},
        {{{SketchConstraintId{40}, SketchConstraintKind::distance, lineRef(1), std::nullopt, 5.0},
          {SketchConstraintId{41}, SketchConstraintKind::horizontal, lineRef(1), std::nullopt, 0.0},
          {SketchConstraintId{42}, SketchConstraintKind::vertical, lineRef(1), std::nullopt, 0.0}},
         43}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    return result.status == voelund::features::SketchSolveStatus::conflicting &&
           !result.conflictingConstraints.empty() &&
           std::ranges::all_of(result.conflictingConstraints, [](SketchConstraintId id) {
               return id.value() >= 40 && id.value() <= 42;
           });
}

bool adapterLocksPointsAndPlacesThemOnLinesAndCircles() {
    using namespace voelund::document;
    const auto start = [](std::uint64_t id) {
        return SketchGeometryReference{SketchElementId{id}, SketchPointPosition::start};
    };
    const auto center = [](std::uint64_t id) {
        return SketchGeometryReference{SketchElementId{id}, SketchPointPosition::center};
    };
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{2.0, 3.0}, {4.0, 3.0}}},
          {SketchElementId{2}, SketchLine{{0.0, 0.0}, {10.0, 0.0}}},
          {SketchElementId{3}, SketchLine{{8.0, 4.0}, {9.0, 4.0}}},
          {SketchElementId{4}, SketchCircle{{8.0, 0.0}, 2.0}}},
         5},
        {{{SketchConstraintId{1}, SketchConstraintKind::coordinateX, start(2), std::nullopt, 0.0},
          {SketchConstraintId{2}, SketchConstraintKind::coordinateY, start(2), std::nullopt, 0.0},
          {SketchConstraintId{3},
           SketchConstraintKind::coordinateX,
           {SketchElementId{2}, SketchPointPosition::end},
           std::nullopt,
           10.0},
          {SketchConstraintId{4},
           SketchConstraintKind::coordinateY,
           {SketchElementId{2}, SketchPointPosition::end},
           std::nullopt,
           0.0},
          {SketchConstraintId{5}, SketchConstraintKind::pointOnObject, start(1), start(2), 0.0},
          {SketchConstraintId{6}, SketchConstraintKind::coordinateX, center(4), std::nullopt, 8.0},
          {SketchConstraintId{7}, SketchConstraintKind::coordinateY, center(4), std::nullopt, 0.0},
          {SketchConstraintId{8}, SketchConstraintKind::radius, center(4), std::nullopt, 2.0},
          {SketchConstraintId{9}, SketchConstraintKind::pointOnObject, start(3), center(4), 0.0}},
         10}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& first = std::get<SketchLine>(sketch.elements.values[0].geometry).start;
    const auto& second = std::get<SketchLine>(sketch.elements.values[2].geometry).start;
    const auto& circle = std::get<SketchCircle>(sketch.elements.values[3].geometry);
    return result.status == voelund::features::SketchSolveStatus::success && close(first.y, 0.0) &&
           close(std::hypot(second.x - circle.center.x, second.y - circle.center.y), circle.radius);
}

bool adapterMakesLengthsAndRadiiEqual() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {5.0, 0.0}}},
          {SketchElementId{2}, SketchLine{{0.0, 3.0}, {2.0, 3.0}}},
          {SketchElementId{3}, SketchCircle{{10.0, 0.0}, 2.0}},
          {SketchElementId{4}, SketchCircle{{15.0, 0.0}, 4.0}}},
         5},
        {{{SketchConstraintId{1}, SketchConstraintKind::equal, lineRef(1), lineRef(2), 0.0},
          {SketchConstraintId{2},
           SketchConstraintKind::equal,
           {SketchElementId{3}, SketchPointPosition::center},
           SketchGeometryReference{SketchElementId{4}, SketchPointPosition::center},
           0.0}},
         3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& firstLine = std::get<SketchLine>(sketch.elements.values[0].geometry);
    const auto& secondLine = std::get<SketchLine>(sketch.elements.values[1].geometry);
    const auto& firstCircle = std::get<SketchCircle>(sketch.elements.values[2].geometry);
    const auto& secondCircle = std::get<SketchCircle>(sketch.elements.values[3].geometry);
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(
               std::hypot(firstLine.end.x - firstLine.start.x, firstLine.end.y - firstLine.start.y),
               std::hypot(secondLine.end.x - secondLine.start.x,
                          secondLine.end.y - secondLine.start.y)) &&
           close(firstCircle.radius, secondCircle.radius);
}

bool adapterMakesLinesAndCirclesTangent() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {10.0, 0.0}}},
          {SketchElementId{2}, SketchCircle{{5.0, 3.0}, 2.0}},
          {SketchElementId{3}, SketchCircle{{14.0, 0.0}, 2.0}},
          {SketchElementId{4}, SketchCircle{{19.0, 0.0}, 1.0}}},
         5},
        {{{SketchConstraintId{1}, SketchConstraintKind::tangent, lineRef(1),
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::center}, 0.0},
          {SketchConstraintId{2},
           SketchConstraintKind::tangent,
           {SketchElementId{3}, SketchPointPosition::center},
           SketchGeometryReference{SketchElementId{4}, SketchPointPosition::center},
           0.0}},
         3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& line = std::get<SketchLine>(sketch.elements.values[0].geometry);
    const auto& circle = std::get<SketchCircle>(sketch.elements.values[1].geometry);
    const auto& firstCircle = std::get<SketchCircle>(sketch.elements.values[2].geometry);
    const auto& secondCircle = std::get<SketchCircle>(sketch.elements.values[3].geometry);
    const double lineX = line.end.x - line.start.x;
    const double lineY = line.end.y - line.start.y;
    const double cross =
        lineX * (circle.center.y - line.start.y) - lineY * (circle.center.x - line.start.x);
    const double lineDistance = std::abs(cross) / std::hypot(lineX, lineY);
    const double centerDistance = std::hypot(secondCircle.center.x - firstCircle.center.x,
                                             secondCircle.center.y - firstCircle.center.y);
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(lineDistance, circle.radius) &&
           close(centerDistance, firstCircle.radius + secondCircle.radius);
}

bool adapterMakesPointsSymmetricAboutLine() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{2.0, 2.0}, {3.0, 2.0}}},
          {SketchElementId{2}, SketchLine{{6.0, 3.0}, {7.0, 3.0}}},
          {SketchElementId{3}, SketchLine{{4.0, -5.0}, {4.0, 5.0}}}},
         4},
        {{{SketchConstraintId{1}, SketchConstraintKind::symmetric, lineRef(1), lineRef(2), 0.0,
           SketchGeometryReference{SketchElementId{3}, SketchPointPosition::start}}},
         2}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& first = std::get<SketchLine>(sketch.elements.values[0].geometry).start;
    const auto& second = std::get<SketchLine>(sketch.elements.values[1].geometry).start;
    const auto& axis = std::get<SketchLine>(sketch.elements.values[2].geometry);
    const SketchPoint midpoint{(first.x + second.x) * 0.5, (first.y + second.y) * 0.5};
    const double axisX = axis.end.x - axis.start.x;
    const double axisY = axis.end.y - axis.start.y;
    const double midpointCross =
        axisX * (midpoint.y - axis.start.y) - axisY * (midpoint.x - axis.start.x);
    const double connectingDot = (second.x - first.x) * axisX + (second.y - first.y) * axisY;
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(midpointCross, 0.0) && close(connectingDot, 0.0);
}

bool adapterDrivesPointDistanceAndLineAngle() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {1.0, 0.0}}},
          {SketchElementId{2}, SketchLine{{3.0, 4.0}, {4.0, 4.0}}},
          {SketchElementId{3}, SketchLine{{10.0, 0.0}, {14.0, 0.0}}},
          {SketchElementId{4}, SketchLine{{10.0, 3.0}, {13.0, 4.0}}}},
         5},
        {{{SketchConstraintId{1}, SketchConstraintKind::pointDistance, lineRef(1), lineRef(2),
           10.0},
          {SketchConstraintId{2}, SketchConstraintKind::angle, lineRef(3), lineRef(4), 60.0}},
         3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& firstPoint = std::get<SketchLine>(sketch.elements.values[0].geometry).start;
    const auto& secondPoint = std::get<SketchLine>(sketch.elements.values[1].geometry).start;
    const auto& firstLine = std::get<SketchLine>(sketch.elements.values[2].geometry);
    const auto& secondLine = std::get<SketchLine>(sketch.elements.values[3].geometry);
    const double firstX = firstLine.end.x - firstLine.start.x;
    const double firstY = firstLine.end.y - firstLine.start.y;
    const double secondX = secondLine.end.x - secondLine.start.x;
    const double secondY = secondLine.end.y - secondLine.start.y;
    const double angle =
        std::atan2(firstX * secondY - firstY * secondX, firstX * secondX + firstY * secondY) *
        180.0 / std::numbers::pi;
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(std::hypot(secondPoint.x - firstPoint.x, secondPoint.y - firstPoint.y), 10.0) &&
           close(angle, 60.0);
}

bool adapterDrivesSignedHorizontalAndVerticalDistances() {
    using namespace voelund::document;
    SketchParameters sketch{plane(),
                            {{{SketchElementId{1}, SketchLine{{0.0, 0.0}, {1.0, 0.0}}},
                              {SketchElementId{2}, SketchLine{{3.0, 4.0}, {4.0, 4.0}}},
                              {SketchElementId{3}, SketchLine{{10.0, 1.0}, {11.0, 1.0}}},
                              {SketchElementId{4}, SketchLine{{12.0, 5.0}, {13.0, 5.0}}}},
                             5},
                            {{{SketchConstraintId{1}, SketchConstraintKind::pointDistanceX,
                               lineRef(1), lineRef(2), -5.0},
                              {SketchConstraintId{2}, SketchConstraintKind::pointDistanceY,
                               lineRef(3), lineRef(4), 7.0}},
                             3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& first = std::get<SketchLine>(sketch.elements.values[0].geometry).start;
    const auto& second = std::get<SketchLine>(sketch.elements.values[1].geometry).start;
    const auto& third = std::get<SketchLine>(sketch.elements.values[2].geometry).start;
    const auto& fourth = std::get<SketchLine>(sketch.elements.values[3].geometry).start;
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(second.x - first.x, -5.0) && close(fourth.y - third.y, 7.0);
}

bool adapterConstrainsMidpointsAndPerpendicularBisectors() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchLine{{3.0, 2.0}, {4.0, 2.0}}},
          {SketchElementId{2}, SketchLine{{0.0, 0.0}, {10.0, 0.0}}},
          {SketchElementId{3}, SketchLine{{14.0, -4.0}, {14.0, 4.0}}, true},
          {SketchElementId{4}, SketchLine{{10.0, 0.0}, {20.0, 2.0}}}},
         5},
        {{{SketchConstraintId{1}, SketchConstraintKind::midpoint, lineRef(1), lineRef(2), 0.0},
          {SketchConstraintId{2}, SketchConstraintKind::perpendicularBisector, lineRef(3),
           lineRef(4), 0.0}},
         3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& point = std::get<SketchLine>(sketch.elements.values[0].geometry).start;
    const auto& target = std::get<SketchLine>(sketch.elements.values[1].geometry);
    const auto& axis = std::get<SketchLine>(sketch.elements.values[2].geometry);
    const auto& bisected = std::get<SketchLine>(sketch.elements.values[3].geometry);
    const SketchPoint midpoint{(target.start.x + target.end.x) * 0.5,
                               (target.start.y + target.end.y) * 0.5};
    const double axisX = axis.end.x - axis.start.x;
    const double axisY = axis.end.y - axis.start.y;
    const double targetX = bisected.end.x - bisected.start.x;
    const double targetY = bisected.end.y - bisected.start.y;
    const double firstDistance =
        std::hypot(axis.start.x - bisected.start.x, axis.start.y - bisected.start.y);
    const double secondDistance =
        std::hypot(axis.start.x - bisected.end.x, axis.start.y - bisected.end.y);
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(point.x, midpoint.x) && close(point.y, midpoint.y) &&
           close(axisX * targetX + axisY * targetY, 0.0) && close(firstDistance, secondDistance);
}

bool adapterConstrainsConcentricCirclesAndDiameter() {
    using namespace voelund::document;
    SketchParameters sketch{
        plane(),
        {{{SketchElementId{1}, SketchCircle{{0.0, 0.0}, 2.0}},
          {SketchElementId{2}, SketchCircle{{8.0, 3.0}, 3.0}}},
         3},
        {{{SketchConstraintId{1},
           SketchConstraintKind::concentric,
           {SketchElementId{1}, SketchPointPosition::center},
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::center},
           0.0},
          {SketchConstraintId{2},
           SketchConstraintKind::diameter,
           {SketchElementId{2}, SketchPointPosition::center},
           std::nullopt,
           12.0}},
         3}};

    const auto result = voelund::features::solveSketchConstraints(sketch);
    const auto& first = std::get<SketchCircle>(sketch.elements.values[0].geometry);
    const auto& second = std::get<SketchCircle>(sketch.elements.values[1].geometry);
    return result.status == voelund::features::SketchSolveStatus::success &&
           close(first.center.x, second.center.x) && close(first.center.y, second.center.y) &&
           close(second.radius * 2.0, 12.0);
}

} // namespace

int main() {
    if (!adapterSolvesVoeIundGeometryAndReportsDegreesOfFreedom()) {
        std::cerr << "Völund PlaneGCS adapter solve failed\n";
        return 1;
    }
    if (!adapterMapsPlaneGcsConflictsBackToStableIds()) {
        std::cerr << "Völund PlaneGCS adapter conflict mapping failed\n";
        return 1;
    }
    if (!adapterLocksPointsAndPlacesThemOnLinesAndCircles()) {
        std::cerr << "Völund point constraint adapter failed\n";
        return 1;
    }
    if (!adapterMakesLengthsAndRadiiEqual()) {
        std::cerr << "Völund equal constraint adapter failed\n";
        return 1;
    }
    if (!adapterMakesLinesAndCirclesTangent()) {
        std::cerr << "Völund tangent constraint adapter failed\n";
        return 1;
    }
    if (!adapterMakesPointsSymmetricAboutLine()) {
        std::cerr << "Völund symmetric constraint adapter failed\n";
        return 1;
    }
    if (!adapterDrivesPointDistanceAndLineAngle()) {
        std::cerr << "Völund point distance or angle adapter failed\n";
        return 1;
    }
    if (!adapterDrivesSignedHorizontalAndVerticalDistances()) {
        std::cerr << "Völund directional point distance adapter failed\n";
        return 1;
    }
    if (!adapterConstrainsMidpointsAndPerpendicularBisectors()) {
        std::cerr << "Völund midpoint or perpendicular bisector adapter failed\n";
        return 1;
    }
    if (!adapterConstrainsConcentricCirclesAndDiameter()) {
        std::cerr << "Völund concentric or diameter adapter failed\n";
        return 1;
    }
    return 0;
}
