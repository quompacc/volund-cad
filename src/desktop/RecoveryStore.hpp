#pragma once

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class QLockFile;

// L05: recovery snapshots in their own directory, never next to or instead of the user's file
// (K08). One entry per document ID: <id>.volund holds the native bytes, <id>.meta the file it
// came from, the time and the feature counts. A running instance holds <id>.lock for every
// entry it writes or offers, so a second instance neither overwrites nor offers it; the lock
// of a crashed instance is stale and frees the entry for the next start.
namespace voelund::desktop {

struct RecoverySnapshot final {
    std::string documentId; // 32 hex digits
    std::string bytes;      // native document
    QString originalFile;   // empty for a document never saved
    std::size_t features{0};
    std::optional<std::size_t> savedFeatures{};
    // L05b: an open sketch draft, kept apart in <id>.draft (the document with the draft
    // applied as feature draftFeature; draftEdits: it replaces that existing sketch).
    std::optional<std::string> draft{};
    std::uint64_t draftFeature{0};
    bool draftEdits{false};
};

struct RecoveryEntry final {
    QString documentId;
    QString originalFile;
    QDateTime savedAt;
    std::size_t features{0};
    std::optional<std::size_t> savedFeatures{};
    // The original was saved after this snapshot, so the snapshot is older than the file.
    bool olderThanFile{false};
    // Metadata or data unreadable: it can only be discarded.
    bool damaged{false};
    bool hasDraft{false};
    std::uint64_t draftFeature{0};
    bool draftEdits{false};
};

class RecoveryStore final {
  public:
    // The directory comes from VOELUND_RECOVERY_DIR, else the application's local data
    // location; it is created on the first write.
    static QString defaultDirectory();
    explicit RecoveryStore(QString directory);
    ~RecoveryStore();
    RecoveryStore(const RecoveryStore&) = delete;
    RecoveryStore& operator=(const RecoveryStore&) = delete;

    [[nodiscard]] const QString& directory() const noexcept { return directory_; }
    // Writes data, then metadata, each atomically; throws on a write error. An entry locked
    // by another running instance is not written (returns false).
    bool write(const RecoverySnapshot& snapshot);
    // Removes this instance's entry (saved, or closed without saving on purpose).
    void remove(const std::string& documentId);
    // Entries no running instance holds; they stay locked by this store until removed or
    // released. Leftover temporary files of an interrupted write are removed first.
    [[nodiscard]] std::vector<RecoveryEntry> candidates();
    [[nodiscard]] std::string read(const RecoveryEntry& entry) const;
    [[nodiscard]] std::string readDraft(const RecoveryEntry& entry) const;
    // Gives up the locks of offered entries that were neither restored nor discarded.
    void release(const QString& documentId);

  private:
    [[nodiscard]] bool lock(const QString& documentId);
    [[nodiscard]] QString path(const QString& documentId, const char* suffix) const;
    void removeStaleTemporaryFiles() const;

    QString directory_;
    std::map<QString, std::unique_ptr<QLockFile>> locks_;
};

} // namespace voelund::desktop
