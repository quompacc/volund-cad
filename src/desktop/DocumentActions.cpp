#include "DocumentActions.hpp"

#include <voelund/application/CadSession.hpp>

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>

#include <exception>
#include <filesystem>
#include <utility>

namespace voelund::desktop {
namespace {

QString documentFilter() { return QStringLiteral("Völund CAD Document (*.volund)"); }

std::filesystem::path nativePath(const QString& fileName) {
#ifdef _WIN32
    return std::filesystem::path(fileName.toStdWString());
#else
    return std::filesystem::path(fileName.toUtf8().constData());
#endif
}

QString displayPath(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.native());
#else
    return QString::fromUtf8(path.native().c_str());
#endif
}

} // namespace

DocumentActions::DocumentActions(QMainWindow& window, application::CadSession& session,
                                 std::function<void()> refreshHandler)
    : window_(window), session_(session), refreshHandler_(std::move(refreshHandler)) {
    QMenu* fileMenu = window_.menuBar()->addMenu(QStringLiteral("&File"));
    QAction* newAction = fileMenu->addAction(QStringLiteral("&New"));
    newAction->setShortcut(QKeySequence::New);
    QAction* openAction = fileMenu->addAction(QStringLiteral("&Open…"));
    openAction->setShortcut(QKeySequence::Open);
    QAction* saveAction = fileMenu->addAction(QStringLiteral("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    QAction* saveAsAction = fileMenu->addAction(QStringLiteral("Save &As…"));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    QObject::connect(newAction, &QAction::triggered, &window_, [this]() { newDocument(); });
    QObject::connect(openAction, &QAction::triggered, &window_, [this]() { open(); });
    QObject::connect(saveAction, &QAction::triggered, &window_,
                     [this]() { static_cast<void>(save()); });
    QObject::connect(saveAsAction, &QAction::triggered, &window_,
                     [this]() { static_cast<void>(saveAs()); });
    syncWindowTitle();
}

bool DocumentActions::confirmClose() { return confirmSaveChanges(); }

void DocumentActions::syncWindowTitle() {
    const QString currentFile = displayPath(session_.currentFile());
    const QString name =
        currentFile.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(currentFile).fileName();
    window_.setWindowTitle(
        QStringLiteral("%1%2 — Völund CAD")
            .arg(name, session_.isModified() ? QStringLiteral(" *") : QString{}));
}

void DocumentActions::newDocument() {
    if (!confirmSaveChanges()) {
        return;
    }
    session_.newDocument();
    refreshHandler_();
    window_.statusBar()->showMessage(QStringLiteral("New document created"), 3000);
}

void DocumentActions::open() {
    if (!confirmSaveChanges()) {
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
        &window_, QStringLiteral("Open Völund Document"), {}, documentFilter());
    if (fileName.isEmpty()) {
        return;
    }
    try {
        session_.loadDocument(nativePath(fileName));
        refreshHandler_();
        window_.statusBar()->showMessage(QStringLiteral("Document loaded"), 3000);
    } catch (const std::exception& error) {
        showError(QStringLiteral("Open failed"), error);
    }
}

bool DocumentActions::confirmSaveChanges() {
    if (!session_.isModified()) {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        &window_, QStringLiteral("Unsaved changes"),
        QStringLiteral("Save changes to the current document?"),
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
    return saveTo(displayPath(session_.currentFile()));
}

bool DocumentActions::saveAs() {
    QString fileName =
        QFileDialog::getSaveFileName(&window_, QStringLiteral("Save Völund Document"),
                                     displayPath(session_.currentFile()), documentFilter());
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
        window_.statusBar()->showMessage(QStringLiteral("Document saved"), 3000);
        return true;
    } catch (const std::exception& error) {
        showError(QStringLiteral("Save failed"), error);
        return false;
    }
}

void DocumentActions::showError(const QString& operation, const std::exception& error) {
    QMessageBox::critical(&window_, operation, QString::fromUtf8(error.what()));
}

} // namespace voelund::desktop
