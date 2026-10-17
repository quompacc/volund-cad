#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U07 (A-IO-01, A-UI-03) in the real main window: New and Close of a dirty document offer
// Save/Discard/Cancel; Cancel keeps the model, a failed Save keeps the window open; a file
// argument takes the safe Open path, a broken one keeps the active document; paths with
// spaces and umlauts work.
namespace {
using namespace voelund;
using test::require;

std::filesystem::path native(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toUtf8().constData());
#endif
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}

// Answers the dialogs the way a user would: the save prompt with `answer`, file dialogs with
// `chosen`, error messages with OK (counted).
struct User final {
    QMessageBox::StandardButton answer{QMessageBox::Cancel};
    QString chosen;
    int prompts{0};
    int errors{0};
    QTimer timer;
    User() {
        QObject::connect(&timer, &QTimer::timeout, [this] {
            auto* modal = QApplication::activeModalWidget();
            if (auto* dialog = qobject_cast<QFileDialog*>(modal)) {
                dialog->selectFile(chosen);
                if (auto* edit = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                    edit->setText(chosen);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            } else if (auto* message = qobject_cast<QMessageBox*>(modal)) {
                if (message->standardButtons().testFlag(QMessageBox::Discard)) {
                    ++prompts;
                    message->button(answer)->click();
                } else {
                    ++errors;
                    message->button(QMessageBox::Ok)->click();
                }
            }
        });
        timer.start(30);
    }
};

void run() {
    QTemporaryDir fixture;
    require(fixture.isValid(), "fixture directory");
    const QString folder = fixture.path() + QString::fromUtf8("/Prüfung mit Leerzeichen");
    require(QDir().mkpath(folder), "folder with spaces");
    const QString path = folder + QString::fromUtf8("/Teil äöü.volund");

    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    User user;

    // Save As into the folder with spaces and umlauts, then dirty the document.
    static_cast<void>(test::requireCommittedFeature(session.createBoxFeature()));
    user.chosen = path;
    command(window, "document.save_as").trigger();
    require(!session.isModified() && session.currentFile() == native(path), "Save As");
    static_cast<void>(test::requireCommittedFeature(session.createBoxFeature()));
    const auto dirty = session.identifiedSnapshot();

    // New: Cancel keeps the model; Save stores it, then starts the new document.
    user.answer = QMessageBox::Cancel;
    command(window, "document.new").trigger();
    require(user.prompts == 1 && session.identifiedSnapshot() == dirty && session.isModified(),
            "Cancel on New keeps the model");
    user.answer = QMessageBox::Save;
    command(window, "document.new").trigger();
    require(user.prompts == 2 && session.features().empty() && !session.isModified(),
            "Save on New stores and starts a new document");
    application::CadSession saved;
    saved.loadDocument(native(path));
    require(saved.features().size() == 2, "The saved file has both boxes");

    // A file argument opens through the same path; a broken one keeps the active document.
    window.openFile(path);
    require(session.features().size() == 2 && session.currentFile() == native(path),
            "The file argument opens the document");
    const QString broken = folder + QStringLiteral("/kaputt.volund");
    {
        QFile file(broken);
        require(file.open(QIODevice::WriteOnly), "broken fixture");
        file.write("not a document");
    }
    static_cast<void>(test::requireCommittedFeature(session.createBoxFeature()));
    const auto beforeBroken = session.identifiedSnapshot();
    user.answer = QMessageBox::Discard;
    window.openFile(broken);
    // The title keeps naming the active document (now marked modified), not the broken file.
    require(user.errors == 1 && session.identifiedSnapshot() == beforeBroken &&
                session.isModified() &&
                window.windowTitle().startsWith(QString::fromUtf8("Teil äöü.volund")) &&
                !window.windowTitle().contains(QStringLiteral("kaputt")),
            "A broken file keeps the active document");

    // Close: Cancel keeps the window; a failed Save keeps it open; Discard closes.
    user.answer = QMessageBox::Cancel;
    require(!window.close() && window.isVisible() && session.identifiedSnapshot() == beforeBroken,
            "Cancel on Close keeps the window and the model");
    std::filesystem::remove(native(path));
    std::filesystem::create_directory(native(path)); // Saving there must fail.
    user.answer = QMessageBox::Save;
    require(!window.close() && window.isVisible() && user.errors == 2 && session.isModified(),
            "A failed Save on Close keeps the window open");
    user.answer = QMessageBox::Discard;
    require(window.close() && !window.isVisible(), "Discard on Close closes");
    std::cout << "PASS U07 document guards: New/Close Save/Discard/Cancel, failed save, file "
                 "argument, paths with spaces\n";
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
