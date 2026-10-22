// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/workspaces/CommandRegistry.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace voelund::workspaces {

void CommandRegistry::registerModule(std::unique_ptr<WorkspaceModule> module) {
    if (!module || module->id().empty() || module->title().empty()) {
        throw std::invalid_argument("Workspace module identity must not be empty");
    }
    for (const auto& registered : modules_) {
        if (registered->id() == module->id()) {
            throw std::invalid_argument("Workspace module ID is already registered");
        }
        for (const CommandDescriptor& existing : registered->commands()) {
            for (const CommandDescriptor& candidate : module->commands()) {
                if (existing.id == candidate.id) {
                    throw std::invalid_argument("Workspace command ID is already registered");
                }
            }
        }
    }
    std::unordered_set<std::string> commandIds;
    for (const CommandDescriptor& command : module->commands()) {
        if (command.id.empty() || command.title.empty() || !command.trigger ||
            !commandIds.insert(command.id).second) {
            throw std::invalid_argument("Workspace command registration is invalid");
        }
    }
    modules_.push_back(std::move(module));
}

std::span<const std::unique_ptr<WorkspaceModule>> CommandRegistry::modules() const noexcept {
    return modules_;
}

const WorkspaceModule& CommandRegistry::module(std::string_view id) const {
    const auto match = std::ranges::find_if(
        modules_, [id](const auto& candidate) { return candidate->id() == id; });
    if (match == modules_.end()) {
        throw std::out_of_range("Unknown workspace module");
    }
    return **match;
}

} // namespace voelund::workspaces
