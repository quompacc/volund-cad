// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// N02 (user decision 2026-10-21): "Zuletzt geöffnet" in the file menu. Two documents saved
// under umlaut names are on the list, newest first; after New the first entry opens the
// document saved last. A deleted document is named when chosen and leaves the list;
// "Liste leeren" empties it.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

QStringList recent() { return QSettings().value(QStringLiteral("document/recent")).toStringList(); }

QMenu& recentMenu(desktop::MainWindow& main) {
    auto* menu = main.findChild<QMenu*>(QStringLiteral("recentMenu"));
    require(menu != nullptr, "Zuletzt geöffnet in the file menu");
    emit menu->aboutToShow(); // as when the pointer rests on it
    return *menu;
}

void chooseRecent(desktop::MainWindow& main, const QString& file) {
    for (QAction* action : recentMenu(main).actions())
        if (action->data().toString() == file) {
            action->trigger();
            test::settleEvents();
            return;
        }
    throw std::runtime_error("Recent entry missing: " + file.toStdString());
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    test::DialogAnswers user;
    user.answer = QMessageBox::Discard;
    application::CadSession session;
    desktop::ProductOptions options;
    options.offerRecovery = false;
    auto main = desktop::startProduct(session, options);
    main->resize(1300, 860);
    auto* fileButton = main->findChild<QToolButton*>(QStringLiteral("fileMenuButton"));
    auto* rail = main->findChild<QWidget*>(QStringLiteral("toolRail"));
    require(fileButton != nullptr && rail != nullptr, "file menu and rail");
    const auto file = [&](const char* id) {
        test::chooseFromMenu(*fileButton, id);
        test::settleEvents();
    };

    const QString first =
        QDir(directory.path()).absoluteFilePath(QStringLiteral("Erstes Ä.volund"));
    const QString second =
        QDir(directory.path()).absoluteFilePath(QStringLiteral("Zweites Ö.volund"));
    test::useRailTool(*rail, "solid.create.box");
    user.file = first;
    file("document.save_as");
    file("document.new");
    test::useRailTool(*rail, "solid.create.box");
    user.file = second;
    file("document.save_as");
    require(recent() == QStringList({second, first}), "Both on the list, newest first");

    file("document.new");
    require(session.features().empty(), "New document");
    file("document.open_recent"); // the first entry: the document saved last
    require(session.features().size() == 1 &&
                session.currentFile().filename() ==
                    std::filesystem::path(second.toStdU16String()).filename(),
            "The first entry opens the document saved last");

    require(QFile::remove(first), "First document deleted");
    const int before = user.errors;
    chooseRecent(*main, first);
    require(user.errors == before + 1 && recent() == QStringList({second}),
            "A deleted document is named and leaves the list");

    file("document.clear_recent");
    require(recent().isEmpty(), "Liste leeren");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS N02 recent documents: newest first, opened from the list, a deleted "
                     "one named and removed, list cleared"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
