#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <vector>

namespace voelund::features {

enum class SketchSolveStatus { success, conflicting, failed };

struct SketchSolveResult final {
    SketchSolveStatus status{SketchSolveStatus::failed};
    int degreesOfFreedom{-1};
    std::vector<document::SketchConstraintId> conflictingConstraints;
};

[[nodiscard]] SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch);

} // namespace voelund::features
