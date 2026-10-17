#include "../../src/desktop/AutosaveController.hpp"
#include "../../src/desktop/RecoveryStore.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/document/DocumentId.hpp>

// L05 (A-RECOVERY-01): state A saved, B changed, autosave, the process goes away. The original
// still holds A; the next start offers B with time and feature counts; restoring gives an
// unsaved document that Save As writes to a new file. An entry of a running instance is not
// offered; a damaged or foreign snapshot never blocks normal opening; an older snapshot is
// marked; a write error is reported and changes nothing. L05b: an open sketch draft is kept
// apart and comes back in the editor.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::filesystem::path native(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toUtf8().constData());
#endif
}
QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read " + path.toStdString());
    return file.readAll();
}
std::string hex(const application::CadSession& session) {
    return document::documentIdToHex(session.documentId());
}
// Polls the store by hand; the timer interval keeps it from running on its own.
constexpr desktop::AutosaveController::Timing manual{3600000, 0, 0};

struct Fixture final {
    QTemporaryDir root;
    QString recovery;
    QString original;
    application::CadSession session;
    QByteArray stateA;
    Fixture() {
        require(root.isValid(), "fixture");
        recovery = root.path() + QStringLiteral("/recovery");
        original = root.path() + QString::fromUtf8("/Halter äöü.volund");
        session.newDocument();
        static_cast<void>(test::requireCommittedFeature(session.createBoxFeature()));
        session.saveDocument(native(original));
        stateA = bytes(original);
        static_cast<void>(test::requireCommittedFeature(session.createBoxFeature()));
    }
};

void crashRecoveryRestoresWithoutTouchingTheOriginal() {
    Fixture fixture;
    auto& session = fixture.session;
    {
        desktop::RecoveryStore store(fixture.recovery);
        desktop::AutosaveController autosave(session, store, manual, [](const QString&) {});
        autosave.poll();
        desktop::RecoveryStore secondInstance(fixture.recovery);
        require(secondInstance.candidates().empty(),
                "An entry of a running instance is not offered elsewhere");
    } // The process goes away without a regular close; its lock is released or stale.
    require(bytes(fixture.original) == fixture.stateA, "The original file still holds A");

    desktop::RecoveryStore next(fixture.recovery);
    const auto found = next.candidates();
    require(found.size() == 1 && !found[0].damaged && !found[0].olderThanFile &&
                found[0].documentId == QString::fromStdString(hex(session)) &&
                native(found[0].originalFile) == native(fixture.original) &&
                found[0].features == 2 && found[0].savedFeatures == 1u &&
                found[0].savedAt.isValid(),
            "The next start offers B with file, time and feature counts");

    application::CadSession restored;
    restored.newDocument();
    restored.restoreDocument(next.read(found[0]), session.documentId());
    require(restored.isModified() && restored.currentFile().empty() &&
                restored.features().size() == 2 && restored.documentId() == session.documentId(),
            "Restore gives an unsaved document without a file");
    const QString copy = fixture.root.path() + QStringLiteral("/Halter wiederhergestellt.volund");
    restored.saveDocument(native(copy));
    require(!restored.isModified() && bytes(fixture.original) == fixture.stateA,
            "Save As writes a new file; the original is unchanged");

    application::CadSession other;
    other.newDocument();
    const auto before = other.identifiedSnapshot();
    bool foreign = false;
    try {
        other.restoreDocument(next.read(found[0]), document::generateDocumentId());
    } catch (const std::exception&) {
        foreign = true;
    }
    require(foreign && other.identifiedSnapshot() == before,
            "A snapshot of another document is rejected before anything changes");
    next.remove(found[0].documentId.toStdString());
    require(desktop::RecoveryStore(fixture.recovery).candidates().empty(),
            "Discard removes only the recovery entry");
    require(bytes(fixture.original) == fixture.stateA && bytes(copy).size() > 0,
            "Discard touches no document file");
}

void savingRemovesTheEntryAndForeignEntriesStay() {
    Fixture fixture;
    auto& session = fixture.session;
    desktop::RecoveryStore store(fixture.recovery);
    // An entry left by another, crashed instance for an unrelated document.
    application::CadSession crashed;
    crashed.newDocument();
    static_cast<void>(test::requireCommittedFeature(crashed.createBoxFeature()));
    {
        desktop::RecoveryStore earlier(fixture.recovery);
        require(earlier.write({hex(crashed), crashed.recoverySnapshot(), {}, 1, std::nullopt}),
                "earlier entry");
    }
    desktop::AutosaveController autosave(session, store, manual, [](const QString&) {});
    autosave.poll();
    const QString own = fixture.recovery + QLatin1Char('/') + QString::fromStdString(hex(session));
    require(QFile::exists(own + QStringLiteral(".volund")), "Unsaved work is written");
    session.saveDocument(native(fixture.original));
    autosave.poll();
    require(!QFile::exists(own + QStringLiteral(".volund")) &&
                !QFile::exists(own + QStringLiteral(".meta")),
            "Saving removes the entry");
    session.newDocument();
    autosave.poll();
    autosave.forgetCurrent();
    const auto left = desktop::RecoveryStore(fixture.recovery).candidates();
    require(left.size() == 1 && left[0].documentId == QString::fromStdString(hex(crashed)) &&
                left[0].savedFeatures == std::nullopt,
            "Entries of other documents are never removed silently");
}

void damagedOlderStaleAndFailedWrites() {
    Fixture fixture;
    auto& session = fixture.session;
    QDir().mkpath(fixture.recovery);
    // Damaged: metadata without data, and garbage data.
    const QString damaged = fixture.recovery + QStringLiteral("/0123456789abcdef0123456789abcdef");
    {
        QFile meta(damaged + QStringLiteral(".meta"));
        require(meta.open(QIODevice::WriteOnly), "damaged fixture");
        meta.write("format=1\nnot really\n");
    }
    // Stale temporary file of an interrupted write, and one of a write in progress.
    const QString stale = fixture.recovery + QStringLiteral("/.voelund-save-1-2-3.tmp");
    const QString fresh = fixture.recovery + QStringLiteral("/.voelund-save-4-5-6.tmp");
    for (const auto& name : {stale, fresh}) {
        QFile file(name);
        require(file.open(QIODevice::WriteOnly), "temporary fixture");
    }
    std::filesystem::last_write_time(native(stale), std::filesystem::file_time_type::clock::now() -
                                                        std::chrono::hours(2));
    {
        desktop::RecoveryStore store(fixture.recovery);
        require(store.write({hex(session), session.recoverySnapshot(),
                             QString::fromStdU16String(session.currentFile().u16string()), 2, 1}),
                "entry");
    }
    // The original is saved again after the snapshot: the snapshot is older.
    std::filesystem::last_write_time(native(fixture.original),
                                     std::filesystem::file_time_type::clock::now() +
                                         std::chrono::minutes(5));
    desktop::RecoveryStore next(fixture.recovery);
    const auto found = next.candidates();
    require(found.size() == 2, "Damaged and valid entries are listed");
    for (const auto& entry : found) {
        if (entry.documentId == QString::fromStdString(hex(session))) {
            require(!entry.damaged && entry.olderThanFile, "An older snapshot is marked");
        } else {
            bool unreadable = false;
            try {
                static_cast<void>(next.read(entry));
            } catch (const std::exception&) {
                unreadable = true;
            }
            require(entry.damaged && unreadable, "A damaged entry can only be discarded");
        }
    }
    require(!QFile::exists(stale) && QFile::exists(fresh),
            "Stale temporary files go, a fresh one stays");
    application::CadSession opened;
    opened.loadDocument(native(fixture.original));
    require(opened.features().size() == 1, "A damaged snapshot does not block normal opening");

    // Write error: the recovery directory is a file. The error is reported once.
    const QString blocked = fixture.root.path() + QStringLiteral("/blocked");
    {
        QFile file(blocked);
        require(file.open(QIODevice::WriteOnly), "blocked fixture");
    }
    desktop::RecoveryStore failing(blocked);
    int reports = 0;
    desktop::AutosaveController autosave(session, failing, manual,
                                         [&reports](const QString&) { ++reports; });
    const auto snapshot = session.identifiedSnapshot();
    autosave.poll();
    autosave.saveNow();
    require(reports == 1 && session.identifiedSnapshot() == snapshot && session.isModified(),
            "A failed write is reported once and changes nothing");
}

void dialogAtStartRestores() {
    Fixture fixture;
    {
        desktop::RecoveryStore store(fixture.recovery);
        require(store.write({hex(fixture.session), fixture.session.recoverySnapshot(),
                             QString::fromStdU16String(fixture.session.currentFile().u16string()),
                             2, 1}),
                "entry for the dialog");
    }
    qputenv("VOELUND_RECOVERY_DIR", fixture.recovery.toUtf8());
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    QString shown;
    QStringList messages;
    int dialogs = 0;
    const QString capture = qEnvironmentVariable("VOELUND_RECOVERY_ARTIFACT_DIR");
    QTimer user;
    QObject::connect(&user, &QTimer::timeout, [&] {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            messages += message->text();
            message->button(QMessageBox::Ok)->click();
            return;
        }
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QLatin1String("recoveryDialog") ||
            dialog->property("answered").toBool())
            return;
        dialog->setProperty("answered", true);
        ++dialogs;
        auto* list = dialog->findChild<QListWidget*>(QStringLiteral("recoveryList"));
        shown = list && list->count() == 1 ? list->item(0)->text() : QString();
        if (!capture.isEmpty() && QDir().mkpath(capture))
            static_cast<void>(
                dialog->grab().save(capture + QStringLiteral("/recovery-dialog.png")));
        // Outside this timer slot, so a message the click opens is answered as well.
        QTimer::singleShot(0, dialog->findChild<QPushButton*>(QStringLiteral("recoveryRestore")),
                           &QPushButton::click);
    });
    user.start(50);
    window.offerRecovery();
    user.stop();
    require(messages.isEmpty(),
            "No error message: " + messages.join(QLatin1Char('|')).toStdString());
    require(dialogs == 1 && shown.contains(QString::fromUtf8("Halter äöü.volund")) &&
                shown.contains(QStringLiteral("1 Features")) && shown.contains(QStringLiteral("2")),
            "The dialog lists the file with its feature counts");
    require(session.isModified() && session.currentFile().empty() &&
                session.features().size() == 2 &&
                session.documentId() == fixture.session.documentId() &&
                window.windowTitle().contains(QLatin1Char('*')),
            "Restore opens B as an unsaved document");
    require(bytes(fixture.original) == fixture.stateA, "The original file is unchanged");
    window.offerRecovery();
    require(dialogs == 1, "A restored entry is not offered again in the same run");
}

// L05b: a sketch on XY with one rectangle (and the same with a second one as the draft).
document::SketchParameters rectangles(int count) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    for (int index = 0; index < count; ++index)
        sketch.elements.values.push_back(
            {document::SketchElementId{static_cast<std::uint64_t>(index + 1)},
             document::SketchRectangle{index * 20.0, 0, index * 20.0 + 10, 6}});
    sketch.elements.nextId = static_cast<std::uint64_t>(count + 1);
    return sketch;
}

// The open draft is unsaved work of its own: it is written apart, also while the document
// itself is unchanged, and goes when the editor closes.
void draftIsWrittenApart() {
    QTemporaryDir root;
    require(root.isValid(), "fixture");
    application::CadSession session;
    session.newDocument();
    desktop::RecoveryStore store(root.path());
    desktop::AutosaveController autosave(session, store, manual, [](const QString&) {});
    std::optional<desktop::SketchEditorToolController::RecoveryDraft> open{
        desktop::SketchEditorToolController::RecoveryDraft{std::nullopt, std::nullopt,
                                                           rectangles(1)}};
    autosave.setDraftProvider([&open] { return open; });
    autosave.poll();
    const QString base = root.path() + QLatin1Char('/') + QString::fromStdString(hex(session));
    require(!session.isModified() && QFile::exists(base + QStringLiteral(".draft")) &&
                QFile::exists(base + QStringLiteral(".volund")),
            "A draft on an unchanged document is written, apart from the document");
    open.reset();
    autosave.poll();
    require(!QFile::exists(base + QStringLiteral(".draft")) &&
                !QFile::exists(base + QStringLiteral(".meta")),
            "A closed editor leaves no entry for an unchanged document");
}

// The whole way: the entry with a draft is restored in the real window, the editor opens
// with the draft, and OK commits exactly that sketch.
void draftRestoresIntoTheEditor(bool edits) {
    QTemporaryDir root;
    require(root.isValid(), "fixture");
    const QString recovery = root.path() + QStringLiteral("/recovery");
    application::CadSession crashed;
    crashed.newDocument();
    std::optional<document::FeatureId> edited;
    if (edits)
        edited = test::requireCommittedFeature(crashed.createDatumSketchFeature(rectangles(1)));
    const std::size_t before = crashed.features().size();
    const auto draft = crashed.sketchDraftSnapshot(edited, std::nullopt, rectangles(2));
    {
        desktop::RecoveryStore store(recovery);
        desktop::RecoverySnapshot snapshot{
            hex(crashed), crashed.recoverySnapshot(), {}, before, std::nullopt};
        snapshot.draft = draft.bytes;
        snapshot.draftFeature = draft.feature.value();
        snapshot.draftEdits = edits;
        require(store.write(snapshot), "entry with draft");
    }
    qputenv("VOELUND_RECOVERY_DIR", recovery.toUtf8());
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    QString shown;
    QTimer user;
    QObject::connect(&user, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QLatin1String("recoveryDialog") ||
            dialog->property("answered").toBool())
            return;
        dialog->setProperty("answered", true);
        shown = dialog->findChild<QListWidget*>(QStringLiteral("recoveryList"))->item(0)->text();
        QTimer::singleShot(0, dialog->findChild<QPushButton*>(QStringLiteral("recoveryRestore")),
                           &QPushButton::click);
    });
    user.start(50);
    window.offerRecovery();
    user.stop();
    QApplication::processEvents();
    require(shown.contains(QStringLiteral("Skizzenentwurf")), "The dialog names the draft");
    require(session.features().size() == before, "The draft is not part of the restored document");
    QPushButton* accept = nullptr;
    for (auto* button : window.findChildren<QPushButton*>())
        if (button->objectName() == QLatin1String("toolAcceptButton") && button->isVisible())
            accept = button;
    require(accept && accept->isEnabled(), "The editor is open with the draft");
    accept->click();
    QApplication::processEvents();
    const auto sketch = edits ? *edited : session.features().back().id;
    require(session.features().size() == (edits ? before : before + 1) &&
                session.sketchParameters(sketch).elements.values.size() >= 2,
            std::string("OK commits the restored draft (") + (edits ? "edit" : "new") + ")");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        crashRecoveryRestoresWithoutTouchingTheOriginal();
        savingRemovesTheEntryAndForeignEntriesStay();
        damagedOlderStaleAndFailedWrites();
        dialogAtStartRestores();
        draftIsWrittenApart();
        draftRestoresIntoTheEditor(false);
        draftRestoresIntoTheEditor(true);
        std::cout << "PASS L05 A-RECOVERY-01 autosave and recovery (with sketch drafts) without "
                     "loss of the original\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
