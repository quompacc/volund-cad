// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>

#include <utility>

namespace voelund::kernel::detail {

// K-GRENZE: runs an OCCT computation and reports its failure as KernelError, so that no layer
// above the kernel catches OCCT exceptions itself.
template <class Action> decltype(auto) guarded(KernelOperation operation, Action&& action) {
    try {
        return std::forward<Action>(action)();
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(operation, occtFailureDiagnostic(failure));
    }
}

} // namespace voelund::kernel::detail
