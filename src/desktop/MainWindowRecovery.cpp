#include <voelund/desktop/MainWindow.hpp>
#include "MainWindowState.hpp"
#include "RecoveryDialog.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentId.hpp>
#include <QStatusBar>
#include <set>

// L05: autosave into the recovery directory and the offer of unsaved work at start.
namespace voelund::desktop {

void MainWindow::createRecovery() {
    impl_->recoveryStore = std::make_unique<RecoveryStore>(RecoveryStore::defaultDirectory());
    impl_->autosave = std::make_unique<AutosaveController>(
        impl_->session, *impl_->recoveryStore, AutosaveController::Timing{},
        [this](const QString& message) { statusBar()->showMessage(message, 8000); }, this);
}

void MainWindow::offerRecovery() {
    auto& store = *impl_->recoveryStore;
    auto entries = store.candidates();
    if (entries.empty())
        return;
    std::set<QString> settled;
    const auto restore = [this, &store, &settled](const RecoveryEntry& entry) {
        const bool restored = impl_->documentActions->restore([&] {
            impl_->session.restoreDocument(
                store.read(entry), document::documentIdFromHex(entry.documentId.toStdString()));
        });
        if (restored) {
            impl_->autosave->adoptRestored();
            settled.insert(entry.documentId);
        }
        return restored;
    };
    const auto discard = [&store, &settled](const RecoveryEntry& entry) {
        store.remove(entry.documentId.toStdString());
        settled.insert(entry.documentId);
        return true;
    };
    RecoveryDialog dialog(entries, restore, discard, this);
    dialog.exec();
    // Entries neither restored nor discarded stay for the next start ("Später").
    for (const auto& entry : entries)
        if (!settled.contains(entry.documentId))
            store.release(entry.documentId);
}

} // namespace voelund::desktop
