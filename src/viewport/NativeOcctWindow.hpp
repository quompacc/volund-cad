// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Window.hxx>

#include <cstdint>

namespace voelund::viewport::platform {

[[nodiscard]] occ::handle<Aspect_Window>
makeOcctWindow(const occ::handle<Aspect_DisplayConnection>& displayConnection,
               std::uintptr_t nativeWindowId);

} // namespace voelund::viewport::platform
