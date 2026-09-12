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
    return 0;
}
