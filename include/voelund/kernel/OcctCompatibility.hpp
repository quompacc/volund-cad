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
};

class KernelError final : public std::runtime_error {
  public:
    KernelError(KernelOperation operation, std::string diagnostic);

    [[nodiscard]] KernelOperation operation() const noexcept;
    [[nodiscard]] const std::string& diagnostic() const noexcept;

  private:
    KernelOperation operation_;
    std::string diagnostic_;
};

[[nodiscard]] const char* kernelOperationName(KernelOperation operation) noexcept;
[[nodiscard]] std::string occtFailureDiagnostic(const Standard_Failure& failure);

} // namespace voelund::kernel
