// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <functional>
#include <stdexcept>
#include <string>

// K-GRENZE (2026-10-22): the kernel's own error type. The layers above the kernel catch it and
// never an OCCT exception; this header names no OCCT type.
namespace voelund::kernel {
enum class KernelOperation {
    makeBox,
    makeCylinder,
    applyTransform,
    fuse,
    cut,
    intersect,
    compound,
    makePrism,
    fillet,         // M05
    chamfer,        // M07
    makeRevolution, // M20
    shell,          // M22
    makeSphere,     // M27
    makeCone,
    makeTorus,
    makeWedge,
    makePipe,       // M23
    loft,           // M24
    helix,          // M26
    draft,          // M25
    mirror,         // M09
    pattern,        // M10
    measure,        // M12
    sketchGeometry, // K-GRENZE: curves, edges and faces of a sketch
    nativeDocument, // K-GRENZE: OCAF storage of the document
    evaluation,     // K-GRENZE: an OCCT failure anywhere in a feature's evaluation
};
// M03: why a Boolean refused its result, beyond a failing kernel.
enum class BooleanFailure {
    none,
    disjointJoin,      // the tool does not touch the body: a join would give two solids
    noMaterialRemoved, // the cut tool misses the body or only touches it
    wholeBodyRemoved,  // the cut would remove the entire body
    emptyIntersection, // tool and body share no volume
    filletFailed,      // M05: the kernel cannot round these edges with this radius
    chamferFailed,     // M07: the kernel cannot chamfer these edges with this distance
    shellFailed,       // M22: the body cannot take walls of this thickness
};
class KernelError final : public std::runtime_error {
  public:
    KernelError(KernelOperation operation, std::string diagnostic,
                BooleanFailure failure = BooleanFailure::none);
    [[nodiscard]] KernelOperation operation() const noexcept;
    [[nodiscard]] const std::string& diagnostic() const noexcept;
    [[nodiscard]] BooleanFailure failure() const noexcept { return failure_; }

  private:
    KernelOperation operation_;
    std::string diagnostic_;
    BooleanFailure failure_;
};
// M03: the stable code of a refused Boolean (EvaluationIssue::code), empty otherwise.
[[nodiscard]] inline const char* booleanFailureCode(BooleanFailure failure) noexcept {
    switch (failure) {
    case BooleanFailure::disjointJoin:
        return "join-disjoint";
    case BooleanFailure::noMaterialRemoved:
        return "cut-no-material";
    case BooleanFailure::wholeBodyRemoved:
        return "cut-whole-body";
    case BooleanFailure::emptyIntersection:
        return "intersect-empty";
    case BooleanFailure::filletFailed:
        return "fillet-failed";
    case BooleanFailure::chamferFailed:
        return "chamfer-failed";
    case BooleanFailure::shellFailed:
        return "shell-failed";
    case BooleanFailure::none:
        break;
    }
    return "";
}
[[nodiscard]] const char* kernelOperationName(KernelOperation operation) noexcept;

// Runs `action`. An OCCT failure becomes KernelError(operation, its diagnostic); running out of
// memory and OCCT programming errors pass through unchanged, like every other exception.
void translateOcctFailures(KernelOperation operation, const std::function<void()>& action);
// The same for every OCCT failure, running out of memory too (document storage reports them all).
void translateEveryOcctFailure(KernelOperation operation, const std::function<void()>& action);
} // namespace voelund::kernel
