#include <voelund/modules/solid/SolidWorkspaceModule.hpp>
#include <voelund/workspaces/CommandRegistry.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

voelund::modules::solid::SolidCommandServices services() {
    return {[]() {},
            []() {},
            []() {},
            []() {},
            []() {},
            [](bool) {},
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; }};
}

bool solidModulePublishesOnlyItsAvailableCommands() {
    voelund::modules::solid::SolidWorkspaceModule module(services());
    const auto commands = module.commands();
    return module.id() == "solid" && module.title() == "Solid" && commands.size() == 6 &&
           commands.front().id == "solid.create.box" &&
           commands.front().group == voelund::workspaces::CommandGroup::create &&
           commands[2].id == "solid.create.sketch" &&
           commands[3].id == "solid.create.extrude_profiles" &&
           commands[4].id == "solid.modify.transform" && commands[4].checkable &&
           commands.back().id == "solid.modify.edit_sketch";
}

bool registryOwnsAndResolvesModules() {
    voelund::workspaces::CommandRegistry registry;
    registry.registerModule(
        std::make_unique<voelund::modules::solid::SolidWorkspaceModule>(services()));
    return registry.modules().size() == 1 && registry.module("solid").commands().size() == 6;
}

bool registryRejectsDuplicateModuleIds() {
    voelund::workspaces::CommandRegistry registry;
    registry.registerModule(
        std::make_unique<voelund::modules::solid::SolidWorkspaceModule>(services()));
    try {
        registry.registerModule(
            std::make_unique<voelund::modules::solid::SolidWorkspaceModule>(services()));
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    if (!solidModulePublishesOnlyItsAvailableCommands()) {
        std::cerr << "Solid workspace command publication test failed\n";
        return 1;
    }
    if (!registryOwnsAndResolvesModules()) {
        std::cerr << "Workspace registry resolution test failed\n";
        return 1;
    }
    if (!registryRejectsDuplicateModuleIds()) {
        std::cerr << "Workspace duplicate registration test failed\n";
        return 1;
    }
    return 0;
}
