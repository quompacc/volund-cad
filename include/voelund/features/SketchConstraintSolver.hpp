#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <vector>

namespace voelund::features {

enum class SketchSolveStatus { success, conflicting, failed };

struct SketchSolveResult final {
    SketchSolveStatus status{SketchSolveStatus::failed};
    int degreesOfFreedom{-1};
    std::vector<document::SketchConstraintId> conflictingConstraints;
    // Elements with at least one free parameter; the others are fully determined (user
    // request 2026-10-05: show the state per element).
    std::vector<document::SketchElementId> freeElements{};
};

[[nodiscard]] SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch);

} // namespace voelund::features
