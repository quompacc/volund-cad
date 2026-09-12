#include <voelund/features/SketchConstraintSolver.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

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
    return 0;
}
