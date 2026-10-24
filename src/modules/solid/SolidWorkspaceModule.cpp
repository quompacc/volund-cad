// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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
    if (services.primitive) {
        struct Primitive final {
            const char* id;
            const char* name;
            const char* tip;
        };
        const Primitive primitives[]{
            {"solid.create.sphere", "Kugel", "Kugel anlegen (auf dem gewählten Körper verbunden)"},
            {"solid.create.cone", "Kegel", "Kegel oder Kegelstumpf anlegen"},
            {"solid.create.torus", "Torus", "Torus (Ring) anlegen"},
            {"solid.create.wedge", "Keil", "Keil anlegen (oben schmaler)"}};
        for (int index = 0; index < 4; ++index)
            commands_.push_back({primitives[index].id,
                                 primitives[index].name,
                                 primitives[index].tip,
                                 {},
                                 workspaces::CommandGroup::create,
                                 true,
                                 false,
                                 simpleCommand([run = services.primitive, index] { run(index); }),
                                 services.canPrimitive ? services.canPrimitive : alwaysEnabled()});
    }
    commands_.push_back({"solid.create.sketch",
                         "Skizze",
                         "Skizze auf einer Ebene oder ebenen Fläche anlegen",
                         {},
                         workspaces::CommandGroup::create,
                         true,
                         false,
                         simpleCommand(std::move(services.sketchEditor)),
                         std::move(services.canEditSketch)});
    commands_.push_back(
        {"solid.create.extrude_profiles", "Extrudieren", "Gewählte Skizzenbereiche extrudieren",
         "E", workspaces::CommandGroup::create, true, false,
         simpleCommand(std::move(services.extrudeProfiles)), services.canExtrudeProfiles});
    if (services.helix)
        commands_.push_back(
            {"solid.create.helix",
             "Wendel",
             "Gewählten Skizzenbereich als Wendel um eine Achse führen (Feder, Gewindenut)",
             {},
             workspaces::CommandGroup::create,
             true,
             false,
             simpleCommand(std::move(services.helix)),
             services.canExtrudeProfiles,
             "Wendel"});
    if (services.loft)
        commands_.push_back(
            {"solid.create.loft",
             "Ausformung",
             "Gewählten Skizzenbereich zu einem Bereich einer anderen Skizze ausformen",
             {},
             workspaces::CommandGroup::create,
             true,
             false,
             simpleCommand(std::move(services.loft)),
             services.canExtrudeProfiles,
             "Ausformen"});
    if (services.sweep)
        commands_.push_back({"solid.create.sweep",
                             "Folgen",
                             "Gewählte Skizzenbereiche entlang einer früheren Skizze führen",
                             {},
                             workspaces::CommandGroup::create,
                             true,
                             false,
                             simpleCommand(std::move(services.sweep)),
                             services.canExtrudeProfiles,
                             "Folgen"});
    if (services.revolve)
        commands_.push_back({"solid.create.revolve",
                             "Drehen",
                             "Gewählte Skizzenbereiche um eine Achse drehen (mit Abziehen: Nut)",
                             {},
                             workspaces::CommandGroup::create,
                             true,
                             false,
                             simpleCommand(std::move(services.revolve)),
                             services.canExtrudeProfiles,
                             "Drehen"});
    commands_.push_back(
        {"solid.modify.transform", "Verschieben/Drehen", "Gewählten Körper verschieben oder drehen",
         "M", workspaces::CommandGroup::modify, true, true, std::move(services.transform),
         std::move(services.canTransform), "Bewegen"});
    commands_.push_back(
        {"solid.modify.edit_sketch",
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
        commands_.push_back({"solid.modify.push_pull", "Drücken/Ziehen",
                             "Gewählte ebene Fläche herausziehen oder in den Körper drücken", "Q",
                             workspaces::CommandGroup::modify, true, false,
                             simpleCommand(std::move(services.pushPull)),
                             std::move(services.canPushPull), "Ziehen"});
    if (services.fillet)
        commands_.push_back(
            {"solid.modify.fillet", "Verrunden", "Kanten des gewählten Körpers verrunden", "F",
             workspaces::CommandGroup::modify, true, false,
             simpleCommand(std::move(services.fillet)), services.canFillet, "Verrunden"});
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
    if (services.measure)
        commands_.push_back({"solid.inspect.measure", "Messen",
                             "Längen, Flächen, Volumen, Abstände und Winkel messen", "I",
                             workspaces::CommandGroup::construct, true, false,
                             simpleCommand(std::move(services.measure)), services.canMeasure,
                             "Messen"});
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
                             services.canCombine,
                             "Kombinieren"});
    if (services.mirror)
        commands_.push_back({"solid.modify.mirror",
                             "Spiegeln",
                             "Gewählten Körper an einer Ebene oder Fläche spiegeln",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.mirror)),
                             services.canMirror,
                             "Spiegeln"});
    if (services.patternLinear)
        commands_.push_back({"solid.modify.pattern_linear",
                             "Lineares Muster",
                             "Gewählten Körper entlang einer Richtung vervielfältigen",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.patternLinear)),
                             services.canPattern,
                             "Muster"});
    if (services.patternCircular)
        commands_.push_back({"solid.modify.pattern_circular",
                             "Kreismuster",
                             "Gewählten Körper um eine Achse vervielfältigen",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.patternCircular)),
                             services.canPattern,
                             "Kreismuster"});
    if (services.hole)
        commands_.push_back({"solid.modify.hole", "Bohrung",
                             "Bohrung an der angeklickten Stelle der gewählten Fläche", "H",
                             workspaces::CommandGroup::modify, true, false,
                             simpleCommand(std::move(services.hole)), services.canHole, "Bohrung"});
    if (services.draft)
        commands_.push_back({"solid.modify.draft",
                             "Formschräge",
                             "Flächen um die gewählte neutrale Fläche neigen",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.draft)),
                             services.canHole,
                             "Formschräge"});
    if (services.shell)
        commands_.push_back({"solid.modify.shell",
                             "Wandstärke",
                             "Körper aushöhlen; die gewählte Fläche bleibt offen",
                             {},
                             workspaces::CommandGroup::modify,
                             true,
                             false,
                             simpleCommand(std::move(services.shell)),
                             services.canHole,
                             "Wandstärke"});
}

std::string SolidWorkspaceModule::id() const { return "solid"; }

std::string SolidWorkspaceModule::title() const { return "Volumen"; }

std::span<const workspaces::CommandDescriptor> SolidWorkspaceModule::commands() const {
    return commands_;
}

} // namespace voelund::modules::solid
