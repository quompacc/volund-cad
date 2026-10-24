// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W09 (as in Fusion and SolidCraft): a right click on a body row selects the body and opens a
// menu with its commands (rename, hide, copy, move …); a feature row offers edit, suppress,
// roll back and delete; a base plane offers a sketch.
namespace {
using namespace voelund;
using test::require;

QTreeWidgetItem* row(QTreeWidget& tree, int kind) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->data(0, Qt::UserRole + 2).toInt() == kind)
            return *it;
    return nullptr;
}

// Reads a tree menu's command IDs when it is shown and closes it again. Under Wayland the
// compositor may refuse a popup without a real input event and exec() returns at once, so the
// IDs are taken at the Show event, before that.
class MenuReader final : public QObject {
  public:
    QStringList ids;
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* menu = qobject_cast<QMenu*>(watched);
        if (event->type() == QEvent::Show && menu != nullptr &&
            menu->objectName() == QStringLiteral("treeMenu")) {
            for (auto* action : menu->actions())
                ids << action->property("commandId").toString();
            QTimer::singleShot(0, menu, &QMenu::close);
        }
        return false;
    }
};

// The row is unfolded and scrolled into view first, as a user sees it before a right click.
QStringList menuFor(QTreeWidget& tree, QTreeWidgetItem* item) {
    require(item != nullptr, "The tree has the row");
    for (auto* parent = item->parent(); parent != nullptr; parent = parent->parent())
        parent->setExpanded(true);
    tree.scrollToItem(item);
    QApplication::processEvents();
    MenuReader reader;
    QApplication::instance()->installEventFilter(&reader);
    emit tree.customContextMenuRequested(tree.visualItemRect(item).center());
    QApplication::instance()->removeEventFilter(&reader);
    return reader.ids;
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    require(session.createBoxFeature({20, 20, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(tree != nullptr, "tree");
    const auto body = menuFor(*tree, row(*tree, 3));
    require(body.contains(QStringLiteral("body.rename")) &&
                body.contains(QStringLiteral("body.visibility")) &&
                body.contains(QStringLiteral("solid.modify.transform")),
            "A body row's menu offers its commands");
    const auto feature = menuFor(*tree, row(*tree, 5));
    require(feature.contains(QStringLiteral("solid.modify.edit_sketch")) &&
                feature.contains(QStringLiteral("history.delete")),
            "A feature row's menu offers edit and delete");
    const auto plane = menuFor(*tree, row(*tree, 1));
    require(plane.contains(QStringLiteral("solid.create.sketch")), "A plane offers a sketch");
    std::cout << "PASS W09 tree menus for body, feature and plane rows\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
