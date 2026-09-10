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
};

class WorkspaceModule {
public:
    virtual ~WorkspaceModule() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string title() const = 0;
    [[nodiscard]] virtual std::span<const CommandDescriptor> commands() const = 0;
};

} // namespace voelund::workspaces
