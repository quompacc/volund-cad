// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <functional>
#include <span>
#include <string>

namespace voelund::workspaces {

enum class CommandGroup { create, modify, construct };

struct CommandDescriptor final {
    std::string id;
    std::string title;
    std::string toolTip;
    std::string shortcut;
    CommandGroup group;
    bool quickAccess;
    bool checkable;
    std::function<void(bool)> trigger;
    std::function<bool()> isEnabled;
    // Short label for the tool rail and context bar; empty uses the title (U04).
    std::string shortTitle{};
};

class WorkspaceModule {
  public:
    virtual ~WorkspaceModule() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string title() const = 0;
    [[nodiscard]] virtual std::span<const CommandDescriptor> commands() const = 0;
};

} // namespace voelund::workspaces
