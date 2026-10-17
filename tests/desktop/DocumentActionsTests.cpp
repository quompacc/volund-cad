#include "../../src/desktop/DocumentActions.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
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

namespace voelund::application {
class CadSessionPersistenceTestAccess {
  public:
    static void exhaust(CadSession& session) { session.generation_ = UINT64_MAX; }
};
} // namespace voelund::application

namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
// File actions are found by command ID, not by their (translated) text.
QAction* action(QMainWindow& window, const QString& id) {
    for (auto* item : window.findChildren<QAction*>())
        if (item->property("commandId").toString() == id)
            return item;
    throw std::runtime_error("missing file action: " + id.toStdString());
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
    voelund::test::isolateSettings();
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
        auto answer = QMessageBox::Discard;
        int duplicatePrompts = 0;
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
                if (message->standardButtons().testFlag(QMessageBox::Discard)) {
                    if (message->windowTitle() == QStringLiteral("Dokument duplizieren"))
                        ++duplicatePrompts;
                    message->button(answer)->click();
                } else {
                    ++errors;
                    message->button(QMessageBox::Ok)->click();
                }
            }
        });
        responder.start(50);
        action(window, QStringLiteral("document.save_as"))->trigger();
        require(dialogs == 1 && errors == 0 && session.currentFile() == native(path) &&
                    !session.isModified() &&
                    window.windowTitle().startsWith(QString::fromUtf8("Teil äöü.volund")),
                "Save As Unicode/title");
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        const QString dirtyTitle = window.windowTitle();
        const auto selection = session.selectedFeature();
        cancel = true;
        action(window, QStringLiteral("document.open"))->trigger();
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
        action(window, QStringLiteral("document.open"))->trigger();
        require(errors == 1 && session.isModified() && window.windowTitle() == dirtyTitle &&
                    session.selectedFeature() == selection && session.currentFile() == native(path),
                "failed open changed UI state");
        chosen = path;
        action(window, QStringLiteral("document.open"))->trigger();
        require(!session.isModified() && session.features().size() == 2 &&
                    !window.windowTitle().contains(" *"),
                "open action did not commit");
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        action(window, QStringLiteral("document.save"))->trigger();
        require(!session.isModified() && !window.windowTitle().contains(" *"), "save action state");
        std::filesystem::remove(native(path));
        std::filesystem::create_directory(native(path));
        static_cast<void>(voelund::test::requireCommittedFeature(session.createBoxFeature()));
        actions.syncWindowTitle();
        const auto beforeFailure = window.windowTitle();
        action(window, QStringLiteral("document.save"))->trigger();
        require(errors == 2 && session.isModified() && window.windowTitle() == beforeFailure &&
                    session.currentFile() == native(path),
                "failed save changed UI state");
        // Dirty original cannot be saved over a directory: duplication must stop.
        const auto failedOriginal = session.identifiedSnapshot();
        answer = QMessageBox::Save;
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(errors == 3 && session.identifiedSnapshot() == failedOriginal &&
                    session.currentFile() == native(path) && window.windowTitle() == beforeFailure,
                "failed source save created a duplicate");
        answer = QMessageBox::Cancel;
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(session.identifiedSnapshot() == failedOriginal && session.canUndo() &&
                    window.windowTitle() == beforeFailure,
                "cancel duplicate changed document");
        answer = QMessageBox::Discard;
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(session.documentId() != failedOriginal.id() &&
                    session.identifiedSnapshot().snapshot() == failedOriginal.snapshot() &&
                    session.currentFile().empty() && session.isModified() && !session.canUndo() &&
                    window.windowTitle().startsWith(QStringLiteral("Unbenannt *")),
                "duplicate without saving lost current content or UI state");
        require(std::filesystem::is_directory(native(path)), "duplicate wrote original path");
        const auto unsaved = session.identifiedSnapshot();
        answer = QMessageBox::Save;
        cancel = true;
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(session.identifiedSnapshot() == unsaved && session.currentFile().empty(),
                "cancel source Save-As created duplicate");
        cancel = false;
        chosen = fixture.path() + QStringLiteral("/source-copy.volund");
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(session.documentId() != unsaved.id() && session.currentFile().empty() &&
                    session.identifiedSnapshot().snapshot() == unsaved.snapshot() &&
                    session.isModified() && std::filesystem::is_regular_file(native(chosen)),
                "save source then duplicate failed");
        voelund::application::CadSession savedSource;
        savedSource.loadDocument(native(chosen));
        require(savedSource.identifiedSnapshot().snapshot() == unsaved.snapshot(),
                "source save lost changes");
        chosen = fixture.path() + QStringLiteral("/duplicate.volund");
        const auto duplicateId = session.documentId();
        action(window, QStringLiteral("document.save"))->trigger();
        require(session.documentId() == duplicateId && !session.isModified() &&
                    session.currentFile() == native(chosen),
                "saving duplicate changed namespace");
        const int promptCount = duplicatePrompts;
        const auto clean = session.identifiedSnapshot();
        action(window, QStringLiteral("document.duplicate"))->trigger();
        require(duplicatePrompts == promptCount && session.documentId() != clean.id() &&
                    session.identifiedSnapshot().snapshot() == clean.snapshot(),
                "clean duplicate prompted or lost content");
        // New and Duplicate must report infrastructure exceptions through the existing error
        // dialog.
        answer = QMessageBox::Discard;
        voelund::application::CadSessionPersistenceTestAccess::exhaust(session);
        const auto exhausted = session.identifiedSnapshot();
        const auto exhaustedTitle = window.windowTitle();
        action(window, QStringLiteral("document.duplicate"))->trigger();
        action(window, QStringLiteral("document.new"))->trigger();
        require(errors == 5 && session.identifiedSnapshot() == exhausted &&
                    window.windowTitle() == exhaustedTitle,
                "lifecycle failure escaped or changed UI");
        responder.stop();
        std::cout << "Production file actions: Unicode, cancel, corrupt open, save and failure "
                     "titles passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
