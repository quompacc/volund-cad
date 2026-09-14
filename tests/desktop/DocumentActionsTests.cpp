#include "../../src/desktop/DocumentActions.hpp"
#include "../CommandResultAssertions.hpp"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>

namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
QAction* action(QMainWindow& window, const QString& text) {
    for (auto* item : window.findChildren<QAction*>())
        if (item->text() == text)
            return item;
    throw std::runtime_error("missing file action");
}
std::filesystem::path native(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toUtf8().constData());
#endif
}
} // namespace
int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    try {
        QTemporaryDir fixture;
        require(fixture.isValid(), "fixture directory");
        const QString path = fixture.path() + QString::fromUtf8("/Teil äöü.volund");
        voelund::application::CadSession session;
        QMainWindow window;
        voelund::desktop::DocumentActions* actionsPointer = nullptr;
        voelund::desktop::DocumentActions actions(window, session,
                                                  [&] { actionsPointer->syncWindowTitle(); });
        actionsPointer = &actions;
        window.resize(640, 360);
        window.show();
        QString chosen = path;
        bool cancel = false;
        int dialogs = 0;
        int errors = 0;
        QTimer responder;
        QObject::connect(&responder, &QTimer::timeout, [&] {
            auto* modal = QApplication::activeModalWidget();
            if (auto* dialog = qobject_cast<QFileDialog*>(modal)) {
                ++dialogs;
                if (cancel)
                    dialog->reject();
                else {
                    dialog->selectFile(chosen);
                    if (auto* edit = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                        edit->setText(chosen);
                    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                }
            } else if (auto* message = qobject_cast<QMessageBox*>(modal)) {
                if (message->standardButtons().testFlag(QMessageBox::Discard))
                    message->button(QMessageBox::Discard)->click();
                else {
                    ++errors;
                    message->button(QMessageBox::Ok)->click();
                }
            }
        });
        responder.start(50);
        action(window, QStringLiteral("Save &As…"))->trigger();
        require(dialogs == 1 && errors == 0 && session.currentFile() == native(path) &&
                    !session.isModified() &&
                    window.windowTitle().startsWith(QString::fromUtf8("Teil äöü.volund")),
                "Save As Unicode/title");
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        const QString dirtyTitle = window.windowTitle();
        const auto selection = session.selectedFeature();
        cancel = true;
        action(window, QStringLiteral("&Open…"))->trigger();
        require(session.isModified() && session.selectedFeature() == selection &&
                    window.windowTitle() == dirtyTitle,
                "cancel open changed document");
        cancel = false;
        const QString badPath = fixture.path() + QStringLiteral("/bad.volund");
        {
            QFile file(badPath);
            require(file.open(QIODevice::WriteOnly), "bad fixture");
            file.write("invalid");
        }
        chosen = badPath;
        action(window, QStringLiteral("&Open…"))->trigger();
        require(errors == 1 && session.isModified() && window.windowTitle() == dirtyTitle &&
                    session.selectedFeature() == selection && session.currentFile() == native(path),
                "failed open changed UI state");
        chosen = path;
        action(window, QStringLiteral("&Open…"))->trigger();
        require(!session.isModified() && session.features().size() == 2 &&
                    !window.windowTitle().contains(" *"),
                "open action did not commit");
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        action(window, QStringLiteral("&Save"))->trigger();
        require(!session.isModified() && !window.windowTitle().contains(" *"), "save action state");
        std::filesystem::remove(native(path));
        std::filesystem::create_directory(native(path));
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        const auto beforeFailure = window.windowTitle();
        action(window, QStringLiteral("&Save"))->trigger();
        require(errors == 2 && session.isModified() && window.windowTitle() == beforeFailure &&
                    session.currentFile() == native(path),
                "failed save changed UI state");
        responder.stop();
        std::cout << "Production file actions: Unicode, cancel, corrupt open, save and failure "
                     "titles passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}