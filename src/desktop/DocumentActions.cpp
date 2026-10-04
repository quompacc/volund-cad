#include "DocumentActions.hpp"
#include "MainWindowSupport.hpp"
#include "ShellState.hpp"

#include <voelund/application/CadSession.hpp>

#include <QAbstractButton>
#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>

#include <exception>
#include <filesystem>
#include <utility>

namespace voelund::desktop {
namespace {

QString documentFilter() { return QObject::tr("Völund-CAD-Dokument (*.volund)"); }

std::filesystem::path nativePath(const QString& fileName) {
#ifdef _WIN32
    return std::filesystem::path(fileName.toStdWString());
#else
    return std::filesystem::path(fileName.toUtf8().constData());
#endif
}

} // namespace

DocumentActions::DocumentActions(QMainWindow& window, application::CadSession& session,
                                 std::function<void()> refreshHandler)
    : window_(window), session_(session), refreshHandler_(std::move(refreshHandler)) {
    // The shell header shows this menu (ADR 0012); it no longer lives in a menu bar.
    fileMenu_ = new QMenu(QObject::tr("&Datei"), &window_);
    fileMenu_->setObjectName(QStringLiteral("fileMenu"));
    QMenu* fileMenu = fileMenu_;
    QAction* newAction = fileMenu->addAction(QObject::tr("&Neu"));
    newAction->setShortcut(QKeySequence::New);
    QAction* openAction = fileMenu->addAction(QObject::tr("Ö&ffnen…"));
    openAction->setShortcut(QKeySequence::Open);
    QAction* saveAction = fileMenu->addAction(QObject::tr("&Speichern"));
    saveAction->setShortcut(QKeySequence::Save);
    QAction* saveAsAction = fileMenu->addAction(QObject::tr("Speichern &unter…"));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    QAction* duplicateAction = fileMenu->addAction(QObject::tr("&Duplizieren"));
    duplicateAction->setToolTip(
        QObject::tr("Ungespeicherte Kopie des aktuellen Dokuments anlegen"));
    // Shortcuts need the actions on the window; the menu itself is only shown on demand.
    window_.addActions(fileMenu_->actions());
    setCommandId(*newAction, "document.new");
    setCommandId(*openAction, "document.open");
    setCommandId(*saveAction, "document.save");
    setCommandId(*saveAsAction, "document.save_as");
    setCommandId(*duplicateAction, "document.duplicate");
    QObject::connect(duplicateAction, &QAction::triggered, &window_,
                     [this]() { duplicateDocument(); });
    QObject::connect(newAction, &QAction::triggered, &window_, [this]() { newDocument(); });
    QObject::connect(openAction, &QAction::triggered, &window_, [this]() { open(); });
    QObject::connect(saveAction, &QAction::triggered, &window_,
                     [this]() { static_cast<void>(save()); });
    QObject::connect(saveAsAction, &QAction::triggered, &window_,
                     [this]() { static_cast<void>(saveAs()); });
    syncWindowTitle();
}

bool DocumentActions::confirmClose() { return confirmSaveChanges(); }

QMenu& DocumentActions::menu() const { return *fileMenu_; }

void DocumentActions::syncWindowTitle() {
    window_.setWindowTitle(projectShell(session_, {}).windowTitle());
    if (shellChanged_)
        shellChanged_();
}

void DocumentActions::setShellChangedHandler(std::function<void()> handler) {
    shellChanged_ = std::move(handler);
    syncWindowTitle();
}

void DocumentActions::newDocument() {
    if (!confirmSaveChanges()) {
        return;
    }
    try {
        session_.newDocument();
        refreshHandler_();
        window_.statusBar()->showMessage(QObject::tr("Neues Dokument angelegt"), 3000);
    } catch (const std::exception& error) {
        showError(QObject::tr("Neu fehlgeschlagen"), error);
    }
}

bool DocumentActions::confirmDuplicate() {
    if (!session_.isModified())
        return true;
    QMessageBox prompt(QMessageBox::Warning, QObject::tr("Dokument duplizieren"),
                       QObject::tr("Änderungen am Original vor dem Duplizieren speichern?\n"
                                   "Das Duplikat enthält alle aktuellen Änderungen."),
                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, &window_);
    prompt.button(QMessageBox::Discard)->setText(QObject::tr("Ohne Speichern duplizieren"));
    prompt.setDefaultButton(QMessageBox::Save);
    const auto answer = prompt.exec();
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void DocumentActions::duplicateDocument() {
    if (!confirmDuplicate())
        return;
    try {
        session_.duplicateDocument();
        refreshHandler_();
        syncWindowTitle();
        window_.statusBar()->showMessage(QObject::tr("Ungespeichertes Duplikat angelegt"), 3000);
    } catch (const std::exception& error) {
        showError(QObject::tr("Duplizieren fehlgeschlagen"), error);
    }
}

void DocumentActions::open() {
    if (!confirmSaveChanges()) {
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
        &window_, QObject::tr("Völund-Dokument öffnen"), {}, documentFilter());
    if (fileName.isEmpty()) {
        return;
    }
    try {
        session_.loadDocument(nativePath(fileName));
        refreshHandler_();
        window_.statusBar()->showMessage(QObject::tr("Dokument geladen"), 3000);
    } catch (const std::exception& error) {
        showError(QObject::tr("Öffnen fehlgeschlagen"), error);
    }
}

bool DocumentActions::confirmSaveChanges() {
    if (!session_.isModified()) {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        &window_, QObject::tr("Ungespeicherte Änderungen"),
        QObject::tr("Änderungen am aktuellen Dokument speichern?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    return answer == QMessageBox::Discard || save();
}

bool DocumentActions::save() {
    if (session_.currentFile().empty()) {
        return saveAs();
    }
    return saveTo(nativeDisplayPath(session_.currentFile()));
}

bool DocumentActions::saveAs() {
    QString fileName =
        QFileDialog::getSaveFileName(&window_, QObject::tr("Völund-Dokument speichern"),
                                     nativeDisplayPath(session_.currentFile()), documentFilter());
    if (fileName.isEmpty()) {
        return false;
    }
    if (!fileName.endsWith(QStringLiteral(".volund"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".volund");
    }
    return saveTo(fileName);
}

bool DocumentActions::saveTo(const QString& fileName) {
    try {
        session_.saveDocument(nativePath(fileName));
        syncWindowTitle();
        window_.statusBar()->showMessage(QObject::tr("Dokument gespeichert"), 3000);
        return true;
    } catch (const std::exception& error) {
        showError(QObject::tr("Speichern fehlgeschlagen"), error);
        return false;
    }
}

void DocumentActions::showError(const QString& operation, const std::exception& error) {
    QMessageBox::critical(&window_, operation, QString::fromUtf8(error.what()));
}

} // namespace voelund::desktop
