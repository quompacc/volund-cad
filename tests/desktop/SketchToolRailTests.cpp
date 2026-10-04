#include "../../src/desktop/CommandPalette.hpp"
#include "../../src/desktop/ModelTreePanel.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U13: the tool rail shows the tools of the active function. In the sketch editor it offers
// drawing tools, relations, dimensions and editing as commands with IDs that the command
// search and shortcuts reach as well; after OK/Cancel it returns to the solid tools.
namespace {
using namespace voelund;
using test::require;

QAction* command(QWidget& window, const QString& id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == id)
            return action;
    return nullptr;
}

void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    auto* rail = window.findChild<QFrame*>(QStringLiteral("toolRail"));
    require(rail != nullptr && rail->isVisible(), "Tool rail shown");
    require(command(window, QStringLiteral("sketch.tool.line")) == nullptr ||
                !command(window, QStringLiteral("sketch.tool.line"))->isEnabled(),
            "No sketch tools outside a sketch");

    // Pick XY in the tree and start a sketch.
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        if ((*it)->text(0) == QStringLiteral("XY-Ebene"))
            emit tree->itemClicked(*it, 0);
    command(window, QStringLiteral("solid.create.sketch"))->trigger();
    settle();

    auto* sketchPage = rail->findChild<QWidget*>(QStringLiteral("sketchToolPage"));
    require(sketchPage != nullptr && sketchPage->isVisible(), "The rail shows the sketch tools");
    for (auto* button : rail->findChildren<QToolButton*>())
        if (button->isVisible() && button->defaultAction() != nullptr)
            require(button->defaultAction()
                        ->property("commandId")
                        .toString()
                        .startsWith(QStringLiteral("sketch.")),
                    "No solid tools while sketching");
    for (const char* id :
         {"sketch.tool.select", "sketch.tool.line", "sketch.tool.rectangle", "sketch.tool.circle",
          "sketch.tool.polygon", "sketch.parallel", "sketch.coincident", "sketch.diameter",
          "sketch.trim", "sketch.linear_pattern", "sketch.delete"})
        require(command(window, QString::fromLatin1(id)) != nullptr,
                (std::string("Sketch command missing: ") + id).c_str());

    QAction* rectangle = command(window, QStringLiteral("sketch.tool.rectangle"));
    require(rectangle->isEnabled() && rectangle->shortcut() == QKeySequence(QStringLiteral("R")),
            "Drawing tools are enabled and have shortcuts");
    rectangle->trigger();
    settle();
    require(rectangle->isChecked(), "The rail selects the rectangle tool");
    require(!desktop::CommandPalette::trigger(window, QStringLiteral("sketch.tool.circle")) &&
                command(window, QStringLiteral("sketch.tool.circle"))->isChecked() &&
                !rectangle->isChecked(),
            "The command search selects a tool through the same command");
    require(!command(window, QStringLiteral("sketch.parallel"))->isEnabled() &&
                desktop::CommandPalette::trigger(window, QStringLiteral("sketch.parallel")),
            "Relations wait for a selection");

    // Without the rail the panel offers the tools itself.
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    command(window, QStringLiteral("view.tool_rail"))->trigger();
    settle();
    bool panelTools = false;
    for (auto* button : panel->findChildren<QToolButton*>())
        if (button->isVisible() && button->text() == QStringLiteral("Linie"))
            panelTools = true;
    require(panelTools, "The panel shows its tools when the rail is off");
    command(window, QStringLiteral("view.tool_rail"))->trigger();
    settle();

    panel->findChild<QPushButton*>(QStringLiteral("toolCancelButton"))->click();
    settle();
    require(!sketchPage->isVisible() && !rectangle->isEnabled(),
            "After Cancel the rail returns to the solid tools");
    std::cout << "PASS tool rail follows the sketch mode\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VoelundTest"));
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
