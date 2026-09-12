#include <voelund/modules/solid/SolidWorkspaceModule.hpp>

#include <utility>

namespace voelund::modules::solid {
namespace {

std::function<void(bool)> simpleCommand(std::function<void()> command)
{
    return [command = std::move(command)](bool) { command(); };
}

std::function<bool()> alwaysEnabled()
{
    return []() { return true; };
}

} // namespace

SolidWorkspaceModule::SolidWorkspaceModule(SolidCommandServices services)
{
    commands_.push_back({
        "solid.create.box", "Box", "Create a parametric box", {},
        workspaces::CommandGroup::create, true, false,
        simpleCommand(std::move(services.addBox)), alwaysEnabled()});
    commands_.push_back({
        "solid.create.cylinder", "Cylinder", "Create a parametric cylinder", {},
        workspaces::CommandGroup::create, true, false,
        simpleCommand(std::move(services.addCylinder)),
        std::move(services.canAddCylinder)});
    commands_.push_back({
        "solid.create.sketch", "Sketch",
        "Create a multi-element sketch on a planar face", {},
        workspaces::CommandGroup::create, true, false,
        simpleCommand(std::move(services.sketchEditor)),
        std::move(services.canEditSketch)});
    commands_.push_back({
        "solid.create.sketch_rectangle", "Sketch Rectangle",
        "Create a rectangle sketch and pull it into a solid", {},
        workspaces::CommandGroup::create, true, false,
        simpleCommand(std::move(services.sketchRectangle)),
        std::move(services.canSketch)});
    commands_.push_back({
        "solid.modify.transform", "Move / Rotate",
        "Move or rotate the selected separate body", "M",
        workspaces::CommandGroup::modify, true, true,
        std::move(services.transform), std::move(services.canTransform)});
}

std::string SolidWorkspaceModule::id() const
{
    return "solid";
}

std::string SolidWorkspaceModule::title() const
{
    return "Solid";
}

std::span<const workspaces::CommandDescriptor> SolidWorkspaceModule::commands() const
{
    return commands_;
}

} // namespace voelund::modules::solid
