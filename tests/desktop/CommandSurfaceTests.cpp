// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/CommandPalette.hpp"
#include "../../src/desktop/ContextCommands.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QToolButton>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U04: tool rail, context bar, command search and shortcuts all reach the same action for a
// command ID; unknown IDs are diagnosed; nothing is reachable only through the context bar.
namespace {
using namespace voelund;
using desktop::ShellSelection;
using desktop::ShellTool;
using test::require;

std::string id(const QAction* action) {
    return action->property("commandId").toString().toStdString();
}

std::vector<QAction*> buttons(const QWidget& surface) {
    std::vector<QAction*> result;
    for (auto* button : surface.findChildren<QToolButton*>())
        if (button->isVisibleTo(&surface) && button->defaultAction() != nullptr)
            result.push_back(button->defaultAction());
    return result;
}

QAction* command(desktop::MainWindow& window, const std::string& wanted) {
    for (auto* action : window.findChildren<QAction*>())
        if (id(action) == wanted)
            return action;
    throw std::runtime_error("Command is missing: " + wanted);
}

void contextRules() {
    desktop::ShellState state;
    state.modelCurrent = true;
    auto content = desktop::contextCommands(state);
    require(content.visible && content.primary.empty() &&
                content.commands == std::vector<std::string>{"solid.create.sketch",
                                                             "solid.create.box",
                                                             "solid.create.cylinder"},
            "Empty selection offers sketch and primitives");
    state.selection = ShellSelection::basePlane;
    state.basePlane = document::DatumPlane::xz;
    content = desktop::contextCommands(state);
    require(content.title == QStringLiteral("XZ-Ebene") && content.primary == "solid.create.sketch",
            "A base plane leads to the sketch");
    state.selection = ShellSelection::sketchRegion;
    content = desktop::contextCommands(state);
    require(content.primary == "solid.create.extrude_profiles", "A region leads to extrude");
    state.tool = ShellTool::sketchEditor;
    require(!desktop::contextCommands(state).visible, "The sketch panel replaces the bar");
    state.tool = ShellTool::transform;
    content = desktop::contextCommands(state);
    require(content.transformSettings &&
                content.commands ==
                    std::vector<std::string>{"transform.snap", "solid.modify.transform"},
            "Move/Rotate shows its snap settings");
}

void surfaces() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    auto* rail = window.findChild<QFrame*>(QStringLiteral("toolRail"));
    auto* bar = window.findChild<QFrame*>(QStringLiteral("contextBar"));
    require(rail != nullptr && bar != nullptr && rail->isVisible() && bar->isVisible(),
            "Tool rail and context bar are shown");

    // U14: most commands sit in the flyouts of the rail's groups, hidden until opened.
    std::set<std::string> railIds;
    for (auto* button : rail->findChildren<QToolButton*>())
        if (button->defaultAction() != nullptr)
            railIds.insert(id(button->defaultAction()));
    const std::set<std::string> solid{"solid.create.box",
                                      "solid.create.cylinder",
                                      "solid.create.sphere",
                                      "solid.create.cone",
                                      "solid.create.torus",
                                      "solid.create.wedge",
                                      "solid.create.sketch",
                                      "solid.create.extrude_profiles",
                                      "solid.create.revolve",
                                      "solid.create.sweep",
                                      "solid.create.loft",
                                      "solid.create.helix",
                                      "solid.modify.transform",
                                      "solid.modify.edit_sketch",
                                      "solid.modify.push_pull",
                                      "solid.modify.fillet",
                                      "solid.modify.chamfer",
                                      "solid.modify.hole",
                                      "solid.modify.combine",
                                      "solid.create.datum_plane",
                                      "solid.modify.shell",
                                      "solid.modify.draft",
                                      "solid.create.datum_axis",
                                      "solid.modify.mirror",
                                      "solid.modify.pattern_linear",
                                      "solid.modify.pattern_circular",
                                      "solid.inspect.measure"};
    require(railIds == solid, "The rail shows every solid command");
    require(!command(window, "solid.create.sketch")->isEnabled(),
            "Unfitting commands stay visible but disabled");

    // The context bar uses the window's own actions, and each is reachable elsewhere.
    const auto contextActions = buttons(*bar);
    require(contextActions.size() == 3, "Empty document context bar");
    for (QAction* action : contextActions) {
        require(action == command(window, id(action)), "Context bar uses the shared action");
        require(railIds.contains(id(action)) || action->associatedObjects().contains(&window),
                "A context command is only reachable through the context bar");
    }

    // Command search triggers the same action; unknown IDs are diagnosed.
    const auto unknown = desktop::CommandPalette::trigger(window, QStringLiteral("solid.nope"));
    require(unknown.has_value() && unknown->contains(QStringLiteral("solid.nope")) &&
                session.features().empty(),
            "Unknown command is diagnosed without side effects");
    const auto unavailable =
        desktop::CommandPalette::trigger(window, QStringLiteral("solid.create.sketch"));
    require(unavailable.has_value() && session.features().empty(),
            "Unavailable command is diagnosed without side effects");
    require(!desktop::CommandPalette::trigger(window, QStringLiteral("solid.create.box")),
            "Command search runs the box command");
    require(session.features().size() == 1, "Search created exactly one box");
    QApplication::processEvents();
    std::set<std::string> afterBox;
    for (QAction* action : buttons(*bar))
        afterBox.insert(id(action));
    require(afterBox == std::set<std::string>{"solid.modify.transform", "solid.modify.fillet",
                                              "solid.modify.chamfer", "solid.create.cylinder",
                                              "history.suppress", "history.rollback_here",
                                              "history.delete"}, // M14, M16
            "Context bar follows the selection");

    QAction* search = command(window, "view.command_search");
    require(search->shortcut() == QKeySequence(QStringLiteral("Ctrl+K")) &&
                search->associatedObjects().contains(&window),
            "Ctrl+K opens the command search");
    QAction* toggle = command(window, "view.tool_rail");
    toggle->trigger();
    require(!rail->isVisible(), "The rail can be switched off");
    toggle->trigger();
    require(rail->isVisible(), "The rail can be switched on again");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        contextRules();
        surfaces();
        std::cout << "PASS command surfaces\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
