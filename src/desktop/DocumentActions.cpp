#include "DocumentActions.hpp"
#include "DocumentErrorText.hpp"
#include "MainWindowSupport.hpp"
#include "RecentFiles.hpp"
#include "ShellState.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/MeshExchange.hpp>

#include <QAbstractButton>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>
#include <utility>

namespace voelund::desktop {
namespace {

QString documentFilter() { return QObject::tr("Völund-CAD-Dokument (*.volund)"); }
QString stepFilter() { return QObject::tr("STEP-Datei (*.step *.stp)"); }
// M18: Open takes a STEP file as well, as a new document holding its import.
QString openFilter() {
    return QObject::tr("Völund-CAD-Dokument oder STEP (*.volund *.step *.stp)") +
           QStringLiteral(";;") + documentFilter() + QStringLiteral(";;") + stepFilter();
}
// N03 (user decision 2026-10-22): the export writes STEP, STL or 3MF by the file type.
enum class ExportFormat { step, stl, threeMf };
QString exportFilter() {
    return stepFilter() + QStringLiteral(";;") + QObject::tr("STL für den 3D-Druck (*.stl)") +
           QStringLiteral(";;") + QObject::tr("3MF für den 3D-Druck (*.3mf)");
}
std::optional<ExportFormat> formatOf(const QString& fileName) {
    if (fileName.endsWith(QStringLiteral(".stl"), Qt::CaseInsensitive))
        return ExportFormat::stl;
    if (fileName.endsWith(QStringLiteral(".3mf"), Qt::CaseInsensitive))
        return ExportFormat::threeMf;
    if (fileName.endsWith(QStringLiteral(".step"), Qt::CaseInsensitive) ||
        fileName.endsWith(QStringLiteral(".stp"), Qt::CaseInsensitive))
        return ExportFormat::step;
    return std::nullopt;
}
QString formatName(ExportFormat format) {
    return format == ExportFormat::stl       ? QStringLiteral("STL")
           : format == ExportFormat::threeMf ? QStringLiteral("3MF")
                                             : QStringLiteral("STEP");
}
// A name without a known ending gets the one of the chosen file type.
QString withSuffix(const QString& fileName, const QString& filter) {
    if (formatOf(fileName))
        return fileName;
    return fileName + (filter.contains(QStringLiteral("*.stl"))   ? QStringLiteral(".stl")
                       : filter.contains(QStringLiteral("*.3mf")) ? QStringLiteral(".3mf")
                                                                  : QStringLiteral(".step"));
}

// Coarse, medium or fine; the last choice is offered again.
std::optional<kernel::MeshResolution> askResolution(QWidget& parent) {
    QSettings settings;
    const QStringList items{QObject::tr("Grob (0,1 mm)"), QObject::tr("Mittel (0,025 mm)"),
                            QObject::tr("Fein (0,005 mm)")};
    const int last =
        std::clamp(settings.value(QStringLiteral("export/meshResolution"), 1).toInt(), 0, 2);
    QInputDialog ask(&parent);
    ask.setObjectName(QStringLiteral("meshResolutionQuestion"));
    ask.setWindowTitle(QObject::tr("Auflösung des Netzes"));
    ask.setLabelText(QObject::tr("Wie fein soll das Dreiecksnetz sein (größte Abweichung)?"));
    ask.setComboBoxItems(items);
    ask.setComboBoxEditable(false);
    ask.setTextValue(items[last]);
    if (ask.exec() != QDialog::Accepted)
        return std::nullopt;
    const int chosen = std::max(0, static_cast<int>(items.indexOf(ask.textValue())));
    settings.setValue(QStringLiteral("export/meshResolution"), chosen);
    return static_cast<kernel::MeshResolution>(chosen);
}

void reportExport(QMainWindow& window, const application::StepExportResult& result, bool visible,
                  const QString& format) {
    using application::StepExportStatus;
    if (result.status == StepExportStatus::written) {
        window.statusBar()->showMessage(
            (visible ? QObject::tr("Als %1 (mm) exportiert: %2 sichtbare(r) Körper")
                     : QObject::tr("Als %1 (mm) exportiert: %2 gewählte(r) Körper"))
                .arg(format)
                .arg(result.solids),
            8000);
        return;
    }
    const QString reason = result.status == StepExportStatus::invalidRevision
                               ? QObject::tr("Das Modell hat Fehler.")
                           : result.status == StepExportStatus::noBodies
                               ? QObject::tr("Es ist kein Körper sichtbar.")
                               : QObject::tr("Die Datei konnte nicht geschrieben werden.");
    QMessageBox::critical(&window, QObject::tr("Export fehlgeschlagen"),
                          reason + QStringLiteral("\n\n") + QString::fromStdString(result.message));
}

bool isStepFile(const QString& fileName) {
    return fileName.endsWith(QStringLiteral(".step"), Qt::CaseInsensitive) ||
           fileName.endsWith(QStringLiteral(".stp"), Qt::CaseInsensitive);
}

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
    recent_ = std::make_unique<RecentFiles>(*fileMenu, saveAction, window_,
                                            [this](const QString& file) { openFile(file); });
    QAction* saveAsAction = fileMenu->addAction(QObject::tr("Speichern &unter…"));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    QAction* duplicateAction = fileMenu->addAction(QObject::tr("&Duplizieren"));
    duplicateAction->setToolTip(
        QObject::tr("Ungespeicherte Kopie des aktuellen Dokuments anlegen"));
    // M18 (user decision 2026-10-16): STEP exchange in the file menu.
    fileMenu->addSeparator();
    QAction* importAction = fileMenu->addAction(QObject::tr("&Importieren (STEP)…"));
    importAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAction->setToolTip(QObject::tr("Volumenkörper einer STEP-Datei ins Dokument übernehmen"));
    QAction* exportAction = fileMenu->addAction(QObject::tr("&Exportieren (STEP, STL, 3MF)…"));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    exportAction->setToolTip(
        QObject::tr("Gewählten Körper, sonst alle sichtbaren Körper als STEP, STL oder 3MF (mm) "
                    "schreiben"));
    setCommandId(*importAction, "document.import_step");
    setCommandId(*exportAction, "document.export_step");
    importAction_ = importAction;
    exportAction_ = exportAction;
    QObject::connect(importAction, &QAction::triggered, &window_, [this]() { importStep(); });
    QObject::connect(exportAction, &QAction::triggered, &window_, [this]() { exportStep(); });
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

DocumentActions::~DocumentActions() = default;

bool DocumentActions::confirmClose() { return confirmSaveChanges(); }

QMenu& DocumentActions::menu() const { return *fileMenu_; }

void DocumentActions::syncWindowTitle() {
    window_.setWindowTitle(projectShell(session_, {}).windowTitle());
    // N02: an opened or saved document goes to the top of "Zuletzt geöffnet".
    const QString file = nativeDisplayPath(session_.currentFile());
    if (!file.isEmpty() && file != remembered_ && recent_) {
        recent_->remember(file);
        remembered_ = file;
    }
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
        &window_, QObject::tr("Völund-Dokument öffnen"), {}, openFilter());
    if (fileName.isEmpty()) {
        return;
    }
    load(fileName);
}

void DocumentActions::openFile(const QString& fileName) {
    if (confirmSaveChanges())
        load(fileName);
}

bool DocumentActions::restore(const std::function<void()>& apply) {
    if (!confirmSaveChanges())
        return false;
    try {
        apply();
        refreshHandler_();
        syncWindowTitle();
        window_.statusBar()->showMessage(
            QObject::tr("Ungespeicherte Arbeit wiederhergestellt – mit „Speichern unter“ sichern"),
            8000);
        return true;
    } catch (const std::exception& error) {
        showError(QObject::tr("Wiederherstellen fehlgeschlagen"), error);
        return false;
    }
}

void DocumentActions::load(const QString& fileName) {
    if (isStepFile(fileName)) {
        try {
            const auto result = session_.openStep(nativePath(fileName));
            if (result.status == application::StepImportStatus::imported) {
                refreshHandler_();
                syncWindowTitle();
            }
            reportImport(result);
        } catch (const std::exception& error) {
            showError(QObject::tr("Öffnen fehlgeschlagen"), error);
        }
        return;
    }
    try {
        session_.loadDocument(nativePath(fileName));
        refreshHandler_();
        window_.statusBar()->showMessage(QObject::tr("Dokument geladen"), 3000);
    } catch (const std::exception& error) {
        // L03: a newer or damaged file says so in the user's words.
        QMessageBox::critical(&window_, QObject::tr("Öffnen fehlgeschlagen"),
                              documentOpenErrorText(error.what()));
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

void DocumentActions::syncExchange(bool toolOpen) {
    importAction_->setEnabled(!toolOpen);
    exportAction_->setEnabled(!toolOpen && !session_.validBodies().empty());
}

void DocumentActions::setExportSelection(std::function<std::vector<document::BodyId>()> selection) {
    exportSelection_ = std::move(selection);
}

namespace {
// The file's unit when it is not mm, as the user reads it.
QString convertedUnit(const std::string& unit) {
    const QString name = QString::fromStdString(unit).toLower();
    if (name.isEmpty() || name.contains(QStringLiteral("milli")))
        return {};
    if (name.contains(QStringLiteral("inch")))
        return QObject::tr("Zoll");
    if (name.contains(QStringLiteral("foot")) || name.contains(QStringLiteral("feet")))
        return QObject::tr("Fuß");
    if (name.contains(QStringLiteral("centi")))
        return QObject::tr("Zentimeter");
    if (name == QStringLiteral("metre") || name == QStringLiteral("meter"))
        return QObject::tr("Meter");
    return name;
}
} // namespace

void DocumentActions::reportImport(const application::StepImportResult& result) {
    using application::StepImportStatus;
    if (result.status != StepImportStatus::imported) {
        const QString reason = result.status == StepImportStatus::noSolids
                                   ? QObject::tr("Die Datei enthält keinen gültigen Volumenkörper.")
                               : result.status == StepImportStatus::unreadable
                                   ? QObject::tr("Die Datei ist keine lesbare STEP-Datei.")
                                   : QObject::tr("Die Volumenkörper lassen sich nicht übernehmen.");
        QMessageBox::critical(&window_, QObject::tr("Import fehlgeschlagen"),
                              reason + QStringLiteral("\n\n") +
                                  QString::fromStdString(result.message));
        return;
    }
    QString message = QObject::tr("Importiert: %1 Körper").arg(result.solids);
    if (const auto unit = convertedUnit(result.sourceUnit); !unit.isEmpty())
        message += QObject::tr(" – Datei in %1, in mm umgerechnet").arg(unit);
    window_.statusBar()->showMessage(message, 8000);
    if (result.skippedShapes == 0 && result.invalidSolids == 0)
        return;
    QString hints = message + QStringLiteral(".");
    if (result.skippedShapes > 0)
        hints += QStringLiteral("\n") +
                 QObject::tr("Übersprungen, weil keine Volumenkörper: %1 Flächen, Schalen oder "
                             "Kurven.")
                     .arg(result.skippedShapes);
    if (result.invalidSolids > 0)
        hints +=
            QStringLiteral("\n") + QObject::tr("Nicht übernommen, weil ungültig: %1 Volumenkörper.")
                                       .arg(result.invalidSolids);
    QMessageBox::information(&window_, QObject::tr("Import mit Hinweisen"), hints);
}

void DocumentActions::importStep() {
    const QString fileName = QFileDialog::getOpenFileName(
        &window_, QObject::tr("STEP-Datei importieren"), {}, stepFilter());
    if (fileName.isEmpty())
        return;
    try {
        const auto result = session_.importStep(nativePath(fileName));
        if (result.status == application::StepImportStatus::imported)
            refreshHandler_();
        reportImport(result);
    } catch (const std::exception& error) {
        showError(QObject::tr("Import fehlgeschlagen"), error);
    }
}

void DocumentActions::exportStep() {
    // A-ERR-01: a model with errors is not exported, not even from its last valid display.
    const auto states = session_.featureStates();
    if (std::ranges::any_of(states, [](const application::FeatureRuntimeState& state) {
            return state.status == application::FeatureRuntimeStatus::error ||
                   state.status == application::FeatureRuntimeStatus::blocked;
        })) {
        QMessageBox::critical(&window_, QObject::tr("Export nicht möglich"),
                              QObject::tr("Das Modell hat Fehler. Beheben Sie sie vor dem "
                                          "Export; die angezeigte ältere Form wird nicht "
                                          "exportiert."));
        return;
    }
    const auto bodies = exportSelection_ ? exportSelection_() : std::vector<document::BodyId>{};
    QString suggestion = QStringLiteral("Teil.step");
    if (!session_.currentFile().empty())
        suggestion = nativeDisplayPath(
            std::filesystem::path(session_.currentFile()).replace_extension(".step"));
    QString filter = stepFilter();
    QString fileName = QFileDialog::getSaveFileName(&window_, QObject::tr("Exportieren"),
                                                    suggestion, exportFilter(), &filter);
    if (fileName.isEmpty())
        return;
    fileName = withSuffix(fileName, filter);
    const auto format = formatOf(fileName).value_or(ExportFormat::step);
    std::optional<kernel::MeshResolution> resolution;
    if (format != ExportFormat::step && !(resolution = askResolution(window_)))
        return;
    try {
        const auto path = nativePath(fileName);
        const auto result =
            format == ExportFormat::step
                ? session_.exportStep(path, bodies)
                : application::exportMesh(session_, path,
                                          format == ExportFormat::stl ? kernel::MeshFormat::stl
                                                                      : kernel::MeshFormat::threeMf,
                                          *resolution, bodies);
        reportExport(window_, result, bodies.empty(), formatName(format));
    } catch (const std::exception& error) {
        showError(QObject::tr("Export fehlgeschlagen"), error);
    }
}

} // namespace voelund::desktop
