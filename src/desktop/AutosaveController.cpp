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

AutosaveController::State AutosaveController::current() const {
    return {session_.sceneStatus().document, draft_ ? draft_() : std::nullopt};
}

void AutosaveController::poll() {
    const auto id = document::documentIdToHex(session_.documentId());
    if (id != documentId_) {
        // New, Open or Duplicate replaced the document after the unsaved-changes question.
        if (ownsEntry_)
            store_.remove(documentId_);
        documentId_ = id;
        lastState_.reset();
        writtenState_.reset();
        ownsEntry_ = false;
    }
    auto state = current();
    if (!session_.isModified() && !state.draft) {
        if (ownsEntry_)
            store_.remove(documentId_); // Saved: the file holds the work now.
        ownsEntry_ = false;
        writtenState_.reset();
        lastState_ = std::move(state);
        sincePending_.invalidate();
        return;
    }
    if (state != lastState_) {
        lastState_ = state;
        sinceChange_.start();
        if (!sincePending_.isValid() && state != writtenState_)
            sincePending_.start();
    }
    if (state == writtenState_)
        return;
    const bool quiet = sinceChange_.isValid() && sinceChange_.elapsed() >= timing_.quietMs;
    const bool overdue = sincePending_.isValid() && sincePending_.elapsed() >= timing_.maximumMs;
    if (quiet || overdue)
        saveNow();
}

void AutosaveController::saveNow() {
    auto state = current();
    if (!session_.isModified() && !state.draft)
        return;
    try {
        RecoverySnapshot snapshot{documentId_, session_.recoverySnapshot(),
                                  QString::fromStdU16String(session_.currentFile().u16string()),
                                  session_.features().size(), session_.savedFeatureCount()};
        if (state.draft) {
            auto draft = session_.sketchDraftSnapshot(state.draft->edited, state.draft->support,
                                                      state.draft->sketch);
            snapshot.draft = std::move(draft.bytes);
            snapshot.draftFeature = draft.feature.value();
            snapshot.draftEdits = state.draft->edited.has_value();
        }
        if (!store_.write(snapshot))
            return;
        writtenState_ = std::move(state);
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
    lastState_.reset();
    writtenState_.reset();
    ownsEntry_ = true;
}

void AutosaveController::forgetCurrent() {
    timer_.stop();
    if (ownsEntry_)
        store_.remove(documentId_);
    ownsEntry_ = false;
}

} // namespace voelund::desktop
