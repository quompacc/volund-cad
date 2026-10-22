// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QTemporaryDir>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U11 (A-UI-03, completing desktop.document_guards): the file menu of the header, clicked, with
// a dirty document: New + Discard; Open + Cancel at the question (no file dialog, nothing
// changed); Open + Save (stored, then the chosen file opens); a cancelled file dialog is no
// error and changes nothing; Close + Save stores and closes; Close + Save of an untitled
// document whose Save As dialog is cancelled keeps the window open without an error.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> window;
    Fixture() {
        session.newDocument();
        window = std::make_unique<desktop::MainWindow>(session);
        window->resize(1300, 860);
        window->show();
        auto* viewport = dynamic_cast<viewport::CadViewport*>(window->centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
    }
    void file(const char* id) {
        test::chooseFromMenu(*window->findChild<QToolButton*>(QStringLiteral("fileMenuButton")),
                             id);
    }
    void box() {
        auto* rail = window->findChild<QWidget*>(QStringLiteral("toolRail"));
        require(rail != nullptr, "rail");
        press(test::railButton(*rail, "solid.create.box"));
        require(session.isModified(), "A box dirties the document");
    }
};

std::size_t storedFeatures(const QString& path) {
    application::CadSession stored;
    stored.loadDocument(std::filesystem::path(path.toStdWString()));
    return stored.features().size();
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    const QString first = directory.path() + QStringLiteral("/Erstes Teil.volund");
    const QString second = directory.path() + QStringLiteral("/Zweites Teil.volund");
    test::DialogAnswers user;
    {
        Fixture f;
        // The second file: one box.
        f.box();
        user.file = second;
        f.file("document.save_as");
        require(!f.session.isModified() && storedFeatures(second) == 1, "Second file saved");

        // New + Discard.
        f.box();
        user.answer = QMessageBox::Discard;
        f.file("document.new");
        require(user.prompts == 1 && f.session.features().empty() && !f.session.isModified(),
                "New + Discard starts empty");

        // The first file, then dirty: Open + Cancel at the question asks for no file.
        f.box();
        user.file = first;
        f.file("document.save_as");
        f.box();
        const auto dirty = f.session.identifiedSnapshot();
        const int dialogs = user.fileDialogs;
        user.answer = QMessageBox::Cancel;
        f.file("document.open");
        require(user.prompts == 2 && user.fileDialogs == dialogs &&
                    f.session.identifiedSnapshot() == dirty,
                "Open + Cancel: no file dialog, nothing changed");

        // Open + Save: the first file is stored with both boxes, then the second opens.
        user.answer = QMessageBox::Save;
        user.file = second;
        f.file("document.open");
        require(storedFeatures(first) == 2 && f.session.features().size() == 1 &&
                    f.session.currentFile() == std::filesystem::path(second.toStdWString()) &&
                    user.errors == 0,
                "Open + Save stores the first file and opens the second");

        // A cancelled file dialog is no error and changes nothing.
        const auto opened = f.session.identifiedSnapshot();
        user.file.clear();
        f.file("document.open");
        require(user.errors == 0 && f.session.identifiedSnapshot() == opened,
                "A cancelled Open dialog changes nothing and shows no error");

        // Close + Save stores and closes.
        f.box();
        user.answer = QMessageBox::Save;
        require(f.window->close() && storedFeatures(second) == 2, "Close + Save stores, closes");
    }
    {
        Fixture f;
        f.box();
        user.answer = QMessageBox::Save;
        user.file.clear(); // the Save As dialog is cancelled
        const int errors = user.errors;
        require(!f.window->close() && f.window->isVisible() && f.session.isModified() &&
                    user.errors == errors,
                "Close + Save of an untitled document, Save As cancelled: stays open, no error");
        user.answer = QMessageBox::Discard;
        require(f.window->close(), "Discard closes");
    }
    std::cout << "PASS U11 document changes (A-UI-03) through the file menu\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    int result = 1;
    QTimer::singleShot(0, [&result] {
        try {
            result = run();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        QApplication::exit(result);
    });
    application.exec();
    return result;
}
