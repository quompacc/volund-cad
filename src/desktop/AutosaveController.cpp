#include "AutosaveController.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentId.hpp>

#include <exception>

namespace voelund::desktop {

AutosaveController::AutosaveController(application::CadSession& session, RecoveryStore& store,
                                       Timing timing, std::function<void(const QString&)> report,
                                       QObject* parent)
    : QObject(parent), session_(session), store_(store), timing_(timing),
      report_(std::move(report)) {
    documentId_ = document::documentIdToHex(session_.documentId());
    timer_.setInterval(timing_.pollMs);
    connect(&timer_, &QTimer::timeout, this, [this] { poll(); });
    timer_.start();
}

void AutosaveController::poll() {
    const auto id = document::documentIdToHex(session_.documentId());
    if (id != documentId_) {
        // New, Open or Duplicate replaced the document after the unsaved-changes question.
        if (ownsEntry_)
            store_.remove(documentId_);
        documentId_ = id;
        lastStamp_.reset();
        writtenStamp_.reset();
        ownsEntry_ = false;
    }
    if (!session_.isModified()) {
        if (ownsEntry_)
            store_.remove(documentId_); // Saved: the file holds the work now.
        ownsEntry_ = false;
        writtenStamp_.reset();
        lastStamp_ = session_.sceneStatus().document;
        sincePending_.invalidate();
        return;
    }
    const auto stamp = session_.sceneStatus().document;
    if (stamp != lastStamp_) {
        lastStamp_ = stamp;
        sinceChange_.start();
        if (!sincePending_.isValid() && stamp != writtenStamp_)
            sincePending_.start();
    }
    if (stamp == writtenStamp_)
        return;
    const bool quiet = sinceChange_.isValid() && sinceChange_.elapsed() >= timing_.quietMs;
    const bool overdue = sincePending_.isValid() && sincePending_.elapsed() >= timing_.maximumMs;
    if (quiet || overdue)
        saveNow();
}

void AutosaveController::saveNow() {
    if (!session_.isModified())
        return;
    const auto stamp = session_.sceneStatus().document;
    try {
        RecoverySnapshot snapshot{documentId_, session_.recoverySnapshot(),
                                  QString::fromStdU16String(session_.currentFile().u16string()),
                                  session_.features().size(), session_.savedFeatureCount()};
        if (!store_.write(snapshot))
            return;
        writtenStamp_ = stamp;
        ownsEntry_ = true;
        sincePending_.invalidate();
        failureReported_ = false;
    } catch (const std::exception& error) {
        if (!failureReported_)
            report_(QObject::tr("Automatische Sicherung fehlgeschlagen: %1")
                        .arg(QString::fromUtf8(error.what())));
        failureReported_ = true;
    }
}

void AutosaveController::adoptRestored() {
    documentId_ = document::documentIdToHex(session_.documentId());
    lastStamp_.reset();
    writtenStamp_.reset();
    ownsEntry_ = true;
}

void AutosaveController::forgetCurrent() {
    timer_.stop();
    if (ownsEntry_)
        store_.remove(documentId_);
    ownsEntry_ = false;
}

} // namespace voelund::desktop
