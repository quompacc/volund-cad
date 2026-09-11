#pragma once

#include <Standard_Handle.hxx>
#include <Standard_Version.hxx>

// OCCT 8 introduced the shorter occ::handle spelling. FreeBSD currently
// packages OCCT 7.9, so provide the same spelling without spreading version
// checks throughout the viewport implementation.
#if OCC_VERSION_MAJOR < 8
namespace occ {

template <class T>
using handle = opencascade::handle<T>;

} // namespace occ
#endif
