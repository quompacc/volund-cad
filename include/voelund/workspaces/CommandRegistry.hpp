#pragma once

#include <voelund/workspaces/WorkspaceModule.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace voelund::workspaces {

class CommandRegistry final {
public:
    void registerModule(std::unique_ptr<WorkspaceModule> module);
    [[nodiscard]] std::span<const std::unique_ptr<WorkspaceModule>> modules() const noexcept;
    [[nodiscard]] const WorkspaceModule& module(std::string_view id) const;

private:
    std::vector<std::unique_ptr<WorkspaceModule>> modules_;
};

} // namespace voelund::workspaces
