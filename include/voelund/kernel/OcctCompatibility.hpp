#pragma once

#include <Standard_Failure.hxx>
#include <Standard_Handle.hxx>
#include <Standard_Version.hxx>

#include <stdexcept>
#include <string>

// OCCT 8 introduced the shorter occ::handle spelling. FreeBSD currently
// packages OCCT 7.9, so provide the same spelling without spreading version
// checks throughout the viewport implementation.
#if OCC_VERSION_MAJOR < 8
namespace occ {

template <class T> using handle = opencascade::handle<T>;

} // namespace occ
#endif

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
    fillet,  // M05
    chamfer, // M07
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
    case BooleanFailure::none:
        break;
    }
    return "";
}
[[nodiscard]] const char* kernelOperationName(KernelOperation operation) noexcept;
[[nodiscard]] std::string occtFailureDiagnostic(const Standard_Failure& failure);

} // namespace voelund::kernel
