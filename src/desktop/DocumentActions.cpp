#include "DocumentActions.hpp"
#include "DocumentErrorText.hpp"
#include "MainWindowSupport.hpp"
#include "RecentFiles.hpp"
#include "ShellState.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/ModelExport.hpp>

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
#include <QStringList>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

QString documentFilter() { return QObject::tr("Völund-CAD-Dokument (*.volund)"); }
QString stepFilter() { return QObject::tr("STEP-Datei (*.step *.stp)"); }
// M18: Open takes a STEP file as well, as a new document holding its import.
QString openFilter() {
    return QObject::tr("Völund-CAD-Dokument oder STEP (*.volund *.step *.stp)") +
           QStringLiteral(";;") + documentFilter() + QStringLiteral(";;") + stepFilter();
}
// N03/N04 (user decisions 2026-10-22): the export writes by the file type: bodies as STEP, STL,
// 3MF or IGES, the selected sketch as DXF.
enum class ExportFormat { step, stl, threeMf, iges, dxf };
struct ExportType final {
    ExportFormat format;
    QString name;
    QStringList suffixes;
    QString filter;
};
const std::vector<ExportType>& exportTypes() {
    static const std::vector<ExportType> types{
        {ExportFormat::step, QStringLiteral("STEP"), {".step", ".stp"}, stepFilter()},
        {ExportFormat::stl,
         QStringLiteral("STL"),
         {".stl"},
         QObject::tr("STL für den 3D-Druck (*.stl)")},
        {ExportFormat::threeMf,
         QStringLiteral("3MF"),
         {".3mf"},
         QObject::tr("3MF für den 3D-Druck (*.3mf)")},
        {ExportFormat::iges,
         QStringLiteral("IGES"),
         {".igs", ".iges"},
         QObject::tr("IGES-Datei (*.igs *.iges)")},
        {ExportFormat::dxf,
         QStringLiteral("DXF"),
         {".dxf"},
         QObject::tr("DXF der gewählten Skizze (*.dxf)")}};
    return types;
}
QString exportFilter() {
    QStringList filters;
    for (const auto& type : exportTypes())
        filters.append(type.filter);
    return filters.join(QStringLiteral(";;"));
}
const ExportType* typeOf(const QString& fileName) {
    for (const auto& type : exportTypes())
        for (const auto& suffix : type.suffixes)
            if (fileName.endsWith(suffix, Qt::CaseInsensitive))
                return &type;
    return nullptr;
}
// A name without a known ending gets the one of the chosen file type.
QString withSuffix(const QString& fileName, const QString& filter) {
    if (typeOf(fileName) != nullptr)
        return fileName;
    for (const auto& type : exportTypes())
        if (type.filter == filter)
            return fileName + type.suffixes.front();
    return fileName + QStringLiteral(".step");
}

// The selected feature when it is a sketch.
std::optional<document::FeatureId> selectedSketch(const application::CadSession& session) {
    const auto selected = session.selectedFeature();
    if (!selected)
        return std::nullopt;
    const auto features = session.features();
    const auto found = std::ranges::find(features, *selected, &document::FeatureDescriptor::id);
    return found != features.end() && found->kind == document::FeatureKind::sketch ? selected
                                                                                   : std::nullopt;
}

// A-ERR-01: a model with errors is not exported, not even from its last valid display.
bool modelHasErrors(const application::CadSession& session) {
    const auto states = session.featureStates();
    return std::ranges::any_of(states, [](const application::FeatureRuntimeState& state) {
        return state.status == application::FeatureRuntimeStatus::error ||
               state.status == application::FeatureRuntimeStatus::blocked;
    });
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
            (format == QStringLiteral("DXF")
                 ? QObject::tr("Skizze als %1 (mm) exportiert: %2 Element(e)")
             : visible ? QObject::tr("Als %1 (mm) exportiert: %2 sichtbare(r) Körper")
                       : QObject::tr("Als %1 (mm) exportiert: %2 gewählte(r) Körper"))
                .arg(format)
                .arg(result.solids),
            8000);
        return;
    }
    const QString reason =
        result.status == StepExportStatus::invalidRevision ? QObject::tr("Das Modell hat Fehler.")
        : result.status == StepExportStatus::noBodies ? QObject::tr("Es ist kein Körper sichtbar.")
        : result.status == StepExportStatus::notASketch
            ? QObject::tr("Gewählt ist keine Skizze.")
            : QObject::tr("Die Datei konnte nicht geschrieben werden.");
    QMessageBox::critical(&window, QObject::tr("Export fehlgeschlagen"),
                          reason + QStringLiteral("\n\n") + QString::fromStdString(result.message));
}

// Writes the file of the chosen type.
// What an export writes, and where.
struct ExportRequest final {
    std::filesystem::path path;
    ExportFormat format;
    std::vector<document::BodyId> bodies;
    std::optional<document::FeatureId> sketch;
    std::optional<kernel::MeshResolution> resolution{};
};

// DXF needs a selected sketch, STL and 3MF a resolution; false when the export stops here.
bool completed(QWidget& parent, ExportRequest& request) {
    if (request.format == ExportFormat::dxf && !request.sketch) {
        QMessageBox::information(&parent, QObject::tr("Keine Skizze gewählt"),
                                 QObject::tr("DXF schreibt eine Skizze. Wählen Sie zuerst die "
                                             "Skizze in der Zeitleiste oder im Baum."));
        return false;
    }
    if (request.format == ExportFormat::stl || request.format == ExportFormat::threeMf)
        request.resolution = askResolution(parent);
    return request.resolution.has_value() ||
           (request.format != ExportFormat::stl && request.format != ExportFormat::threeMf);
}

application::StepExportResult writeExport(application::CadSession& session,
                                          const ExportRequest& request) {
    switch (request.format) {
    case ExportFormat::stl:
    case ExportFormat::threeMf:
        return application::exportMesh(session, request.path,
                                       request.format == ExportFormat::stl
                                           ? kernel::MeshFormat::stl
                                           : kernel::MeshFormat::threeMf,
                                       *request.resolution, request.bodies);
    case ExportFormat::iges:
        return application::exportIges(session, request.path, request.bodies);
    case ExportFormat::dxf:
        return application::exportSketchDxf(session, request.path, *request.sketch);
    case ExportFormat::step:
        break;
    }
    return session.exportStep(request.path, request.bodies);
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
    QAction* exportAction = fileMenu->addAction(QObject::tr("&Exportieren…"));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    exportAction->setToolTip(
        QObject::tr("Gewählten Körper, sonst alle sichtbaren Körper als STEP, STL, 3MF oder IGES "
                    "schreiben, eine gewählte Skizze als DXF (mm)"));
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
    const auto sketch = selectedSketch(session_);
    if (!sketch && modelHasErrors(session_)) {
        QMessageBox::critical(&window_, QObject::tr("Export nicht möglich"),
                              QObject::tr("Das Modell hat Fehler. Beheben Sie sie vor dem "
                                          "Export; die angezeigte ältere Form wird nicht "
                                          "exportiert."));
        return;
    }
    const auto bodies = exportSelection_ ? exportSelection_() : std::vector<document::BodyId>{};
    const QString fileName = askExportFile();
    if (fileName.isEmpty())
        return;
    const auto& type = *typeOf(fileName);
    ExportRequest request{nativePath(fileName), type.format, bodies, sketch};
    if (!completed(window_, request))
        return;
    try {
        reportExport(window_, writeExport(session_, request), bodies.empty(), type.name);
    } catch (const std::exception& error) {
        showError(QObject::tr("Export fehlgeschlagen"), error);
    }
}

// The file to export to, with the ending of its type; empty when cancelled.
QString DocumentActions::askExportFile() {
    QString suggestion = QStringLiteral("Teil.step");
    if (!session_.currentFile().empty())
        suggestion = nativeDisplayPath(
            std::filesystem::path(session_.currentFile()).replace_extension(".step"));
    QString filter = stepFilter();
    const QString fileName = QFileDialog::getSaveFileName(&window_, QObject::tr("Exportieren"),
                                                          suggestion, exportFilter(), &filter);
    return fileName.isEmpty() ? fileName : withSuffix(fileName, filter);
}

} // namespace voelund::desktop
