// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <vector>

namespace voelund::features {

enum class SketchSolveStatus { success, conflicting, failed };

struct SketchSolveResult final {
    SketchSolveStatus status{SketchSolveStatus::failed};
    int degreesOfFreedom{-1};
    std::vector<document::SketchConstraintId> conflictingConstraints;
    // S07: consistent relations that another relation already implies. They take no degree
    // of freedom, so the count above stays higher than the sketch's real freedom.
    std::vector<document::SketchConstraintId> redundantConstraints{};
    // Elements with at least one free parameter; the others are fully determined (user
    // request 2026-10-05: show the state per element).
    std::vector<document::SketchElementId> freeElements{};
};

[[nodiscard]] SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch);

// S18 (SketchReferenceDimensions.cpp): dimensions that can be references, and the measured
// values of the reference dimensions of `sketch` (called after each successful solve).
[[nodiscard]] bool isSketchDimension(document::SketchConstraintKind kind) noexcept;
void measureReferenceDimensions(document::SketchParameters& sketch);

} // namespace voelund::features
