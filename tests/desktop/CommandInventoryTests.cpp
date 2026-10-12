#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QMenu>
#include <QToolBar>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U01: every user-visible action has a stable command ID from the command table in
// docs/ui-concept.md, the empty document offers only applicable commands, and a disabled
// command changes neither the document nor the selection.
namespace {
using namespace voelund;
using test::require;

const std::set<std::string> commandTable{"document.new",
                                         "document.open",
                                         "document.save",
                                         "document.save_as",
                                         "document.duplicate",
                                         "history.undo",
                                         "history.redo",
                                         "repair.references",
                                         "repair.all",
                                         "body.rename",
                                         "body.visibility",
                                         "transform.snap",
                                         "view.theme",
                                         "view.command_search",
                                         "view.tool_rail",
                                         "view.grid",
                                         "sketch.show_dimensions",
                                         "sketch.show_projected",
                                         "sketch.auto_relations",
                                         "solid.create.box",
                                         "solid.create.cylinder",
                                         "solid.create.sketch",
                                         "solid.create.extrude_profiles",
                                         "solid.modify.transform",
                                         "solid.modify.edit_sketch",
                                         "solid.modify.push_pull",
                                         "solid.modify.fillet",
                                         "solid.modify.chamfer",
                                         "assistant.mcp",
                                         "assistant.mcp_copy_command",
                                         "assistant.mcp_new_token",
                                         "assistant.allow",
                                         "assistant.panel"};

std::string commandId(const QAction& action) {
    return action.property("commandId").toString().toStdString();
}

// Actions Qt creates for showing docks and toolbars are window chrome, not commands.
bool isChrome(const desktop::MainWindow& window, const QAction* action) {
    for (const auto* dock : window.findChildren<QDockWidget*>())
        if (dock->toggleViewAction() == action)
            return true;
    for (const auto* toolbar : window.findChildren<QToolBar*>())
        if (toolbar->toggleViewAction() == action)
            return true;
    return action->isSeparator() || action->menu() != nullptr || action->text().isEmpty();
}

std::vector<QAction*> commands(const desktop::MainWindow& window) {
    std::vector<QAction*> result;
    for (auto* action : window.findChildren<QAction*>())
        if (!isChrome(window, action))
            result.push_back(action);
    return result;
}

QAction& command(const desktop::MainWindow& window, const std::string& id) {
    for (auto* action : commands(window))
        if (commandId(*action) == id)
            return *action;
    throw std::runtime_error("Command is missing: " + id);
}

void everyActionIsInTheTable(desktop::MainWindow& window) {
    std::set<std::string> seen;
    for (const auto* action : commands(window)) {
        const std::string id = commandId(*action);
        require(!id.empty(),
                ("Action without command ID: " + action->text().toStdString()).c_str());
        require(seen.insert(id).second, ("Duplicate command ID: " + id).c_str());
    }
    require(seen == commandTable, "Visible commands must match the command table");
    // File commands live in the header menu; their shortcuts must still reach the window.
    for (const char* id : {"document.new", "document.open", "document.save"})
        require(
            command(window, id).associatedObjects().contains(&window),
            ("Shortcut of a file command does not reach the window: " + std::string(id)).c_str());
}

void emptyDocumentOffersOnlyApplicableCommands(application::CadSession& session,
                                               desktop::MainWindow& window) {
    std::set<std::string> enabled;
    for (const auto* action : commands(window))
        if (action->isEnabled())
            enabled.insert(commandId(*action));
    const std::set<std::string> expected{
        "document.new",       "document.open",   "document.save", "document.save_as",
        "document.duplicate", "transform.snap",  "view.theme",    "view.command_search",
        "view.tool_rail",     "view.grid",       "solid.create.box",
        "solid.create.cylinder", // M01: as a new body
        "assistant.mcp",
        "assistant.mcp_new_token",
        "assistant.allow", "assistant.panel"};
    require(enabled == expected, "Empty document enables only document commands, Box and Cylinder");
    const auto before = session.sceneStatus().document;
    for (const std::string& id : commandTable) {
        if (enabled.contains(id))
            continue;
        QAction& action = command(window, id);
        action.trigger();
        require(session.sceneStatus().document == before && !session.isModified() &&
                    !session.selectedFeature().has_value(),
                ("Disabled command changed the document: " + id).c_str());
    }
}

void historyDoesNotDependOnSelection(application::CadSession& session,
                                     desktop::MainWindow& window) {
    command(window, "solid.create.box").trigger();
    require(session.features().size() == 1, "Box command creates a box");
    QAction& undo = command(window, "history.undo");
    QAction& redo = command(window, "history.redo");
    require(undo.isEnabled() && !redo.isEnabled(), "Undo after creation");
    undo.trigger();
    require(session.features().empty() && !session.selectedFeature().has_value(),
            "Undo removes the box and leaves nothing selected");
    require(redo.isEnabled(), "Redo stays available without a selection");
    redo.trigger();
    require(session.features().size() == 1, "Redo restores the box");
    require(command(window, "solid.create.cylinder").isEnabled(),
            "Cylinder becomes available once a body exists");
}

void connectionIsEditedOnTheSelectedCylinder(application::CadSession& session,
                                             desktop::MainWindow& window) {
    command(window, "solid.create.cylinder").trigger();
    const auto bodies = session.currentBodies().size();
    require(session.selectedCylinderParameters().resultMode == document::CylinderResultMode::join,
            "A new cylinder joins by default");
    auto* mode = window.findChild<QComboBox*>(QStringLiteral("cylinderResultModeInput"));
    require(mode != nullptr, "Connection lives in the cylinder properties");
    mode->setCurrentIndex(3);
    emit mode->activated(3);
    require(session.selectedCylinderParameters().resultMode ==
                    document::CylinderResultMode::separate &&
                session.currentBodies().size() == bodies + 1,
            "Changing the connection edits the selected cylinder");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        application::CadSession session;
        session.newDocument();
        desktop::MainWindow window(session);
        window.show();
        auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
        everyActionIsInTheTable(window);
        emptyDocumentOffersOnlyApplicableCommands(session, window);
        historyDoesNotDependOnSelection(session, window);
        connectionIsEditedOnTheSelectedCylinder(session, window);
        std::cout << "PASS command inventory\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
