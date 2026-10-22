// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "ShellState.hpp"

#include <QString>

#include <string>
#include <vector>

namespace voelund::desktop {

// U04: what the floating context bar offers for the current selection (ADR 0012). The
// primary command is the next sensible step and is highlighted. Every command listed here
// is also reachable through the tool rail or the command search.
struct ContextCommands final {
    bool visible{true};
    QString title;
    std::vector<std::string> commands;
    std::string primary;
    bool transformSettings{false}; // snap controls of Move/Rotate
};

[[nodiscard]] ContextCommands contextCommands(const ShellState& state);

} // namespace voelund::desktop
