#include <voelund/modules/solid/SolidWorkspaceModule.hpp>

#include <utility>

namespace voelund::modules::solid {
namespace {

std::function<void(bool)> simpleCommand(std::function<void()> command) {
    return [command = std::move(command)](bool) { command(); };
}

std::function<bool()> alwaysEnabled() {
    return []() { return true; };
}

} // namespace

SolidWorkspaceModule::SolidWorkspaceModule(SolidCommandServices services) {
    commands_.push_back({"solid.create.box",
                         "Quader",
                         "Parametrischen Quader anlegen",
                         {},
                         workspaces::CommandGroup::create,
                         true,
                         false,
                         simpleCommand(std::move(services.addBox)),
                         services.canAddBox ? std::move(services.canAddBox) : alwaysEnabled()});
    commands_.push_back({"solid.create.cylinder",
                         "Zylinder",
                         "Parametrischen Zylinder anlegen",
                         {},
                         workspaces::CommandGroup::create,
                         true,
                         false,
                         simpleCommand(std::move(services.addCylinder)),
                         std::move(services.canAddCylinder)});
    commands_.push_back({"solid.create.sketch",
                         "Skizze",
                         "Skizze auf einer Ebene oder ebenen Fläche anlegen",
                         {},
                         workspaces::CommandGroup::create,
                         true,
                         false,
                         simpleCommand(std::move(services.sketchEditor)),
                         std::move(services.canEditSketch)});
    commands_.push_back({"solid.create.extrude_profiles",
                         "Extrudieren",
                         "Gewählte Skizzenbereiche extrudieren",
                         {},
                         workspaces::CommandGroup::create,
                         true,
                         false,
                         simpleCommand(std::move(services.extrudeProfiles)),
                         std::move(services.canExtrudeProfiles)});
    commands_.push_back({"solid.modify.transform", "Verschieben/Drehen",
                         "Gewählten getrennten Körper verschieben oder drehen", "M",
                         workspaces::CommandGroup::modify, true, true,
                         std::move(services.transform), std::move(services.canTransform),
                         "Bewegen"});
    commands_.push_back({"solid.modify.edit_sketch",
                         "Bearbeiten",
                         "Gewähltes Feature bearbeiten (Skizze, Extrusion, Verrundung, Fase, Bohrung)",
                         {},
                         workspaces::CommandGroup::modify,
                         true,
                         false,
                         simpleCommand(std::move(services.editSketch)),
                         std::move(services.canEditExistingSketch),
                         "Bearbeiten"});
    if (services.pushPull)
        commands_.push_back({"solid.modify.push_pull",
                             "Drücken/Ziehen",
                             "Gewählte ebene Fläche herausziehen oder in den Körper drücken",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.pushPull)),
                             std::move(services.canPushPull),
                             "Ziehen"});
    if (services.fillet)
        commands_.push_back({"solid.modify.fillet",
                             "Verrunden",
                             "Kanten des gewählten Körpers verrunden",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.fillet)),
                             services.canFillet,
                             "Verrunden"});
    if (services.chamfer)
        commands_.push_back({"solid.modify.chamfer",
                             "Fase",
                             "Kanten des gewählten Körpers symmetrisch anfasen",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.chamfer)),
                             services.canFillet,
                             "Fase"});
    if (services.datumPlane)
        commands_.push_back({"solid.create.datum_plane",
                             "Bezugsebene",
                             "Ebene parallel zur gewählten Grundebene oder Fläche, versetzt "
                             "oder gedreht",
                             {},
                             workspaces::CommandGroup::create,
                             true,
                             false,
                             simpleCommand(std::move(services.datumPlane)),
                             services.canDatum,
                             "Bezugsebene"});
    if (services.datumAxis)
        commands_.push_back({"solid.create.datum_axis",
                             "Bezugsachse",
                             "Achse auf der gewählten Kante oder einer Grundachse",
                             {},
                             workspaces::CommandGroup::create,
                             true,
                             false,
                             simpleCommand(std::move(services.datumAxis)),
                             services.canDatum,
                             "Bezugsachse"});
    if (services.combine)
        commands_.push_back({"solid.modify.combine",
                             "Kombinieren",
                             "Gewählten Körper mit anderen Körpern vereinigen, von ihnen "
                             "abziehen oder schneiden",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.combine)),
                             services.canFillet,
                             "Kombinieren"});
    if (services.hole)
        commands_.push_back({"solid.modify.hole",
                             "Bohrung",
                             "Bohrung an der angeklickten Stelle der gewählten Fläche",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.hole)),
                             std::move(services.canHole),
                             "Bohrung"});
}

std::string SolidWorkspaceModule::id() const { return "solid"; }

std::string SolidWorkspaceModule::title() const { return "Volumen"; }

std::span<const workspaces::CommandDescriptor> SolidWorkspaceModule::commands() const {
    return commands_;
}

} // namespace voelund::modules::solid
