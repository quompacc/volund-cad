#pragma once

#include "RecoveryStore.hpp"

#include <voelund/application/SceneStatus.hpp>

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <functional>
#include <optional>
#include <string>

namespace voelund::application {
class CadSession;
}

// L05 (user decision 2026-10-07): an unsaved document is written to its recovery entry 30 s
// after the last change, and at the latest every 2 min while changes keep coming. A saved
// document has no entry; one that is closed, replaced or discarded on purpose loses it. A
// failed write is reported once and retried; it never blocks the work.
namespace voelund::desktop {

class AutosaveController final : public QObject {
  public:
    struct Timing final {
        int pollMs{5000};
        qint64 quietMs{30000};
        qint64 maximumMs{120000};
    };
    AutosaveController(application::CadSession& session, RecoveryStore& store, Timing timing,
                       std::function<void(const QString&)> report, QObject* parent = nullptr);
    // One check now (also what the timer runs).
    void poll();
    // Writes the current document's entry now if it has unsaved changes.
    void saveNow();
    // The session now holds a restored recovery entry; it is this instance's from now on.
    void adoptRestored();
    // The window closes after the user decided about unsaved changes.
    void forgetCurrent();

  private:
    application::CadSession& session_;
    RecoveryStore& store_;
    Timing timing_;
    std::function<void(const QString&)> report_;
    QTimer timer_;
    std::string documentId_;
    std::optional<application::DocumentStamp> lastStamp_;
    std::optional<application::DocumentStamp> writtenStamp_;
    QElapsedTimer sinceChange_;
    QElapsedTimer sincePending_;
    bool ownsEntry_{false}; // written or restored by this instance
    bool failureReported_{false};
};

} // namespace voelund::desktop
