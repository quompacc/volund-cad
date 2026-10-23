// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <Standard_Failure.hxx>
#include <Standard_Handle.hxx>
#include <Standard_Version.hxx>
#include <voelund/kernel/KernelError.hpp>

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
[[nodiscard]] std::string occtFailureDiagnostic(const Standard_Failure& failure);

} // namespace voelund::kernel
