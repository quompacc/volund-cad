// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "RecoveryStore.hpp"

#include <voelund/platform/AtomicFileStore.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QStandardPaths>
#include <QStringList>

#include <filesystem>
#include <stdexcept>

namespace voelund::desktop {
namespace {

constexpr int metadataFormat = 1;
// A temporary file this old belongs to an interrupted write, never to one in progress.
constexpr qint64 staleTemporarySeconds = 10 * 60;

std::filesystem::path nativePath(const QString& path) {
    return std::filesystem::path(path.toStdU16String());
}

bool isDocumentId(const QString& text) {
    if (text.size() != 32)
        return false;
    for (const QChar character : text)
        if (!character.isDigit() && (character < u'a' || character > u'f'))
            return false;
    return true;
}

} // namespace

QString RecoveryStore::defaultDirectory() {
    const QString configured = qEnvironmentVariable("VOELUND_RECOVERY_DIR");
    if (!configured.isEmpty())
        return configured;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           QStringLiteral("/recovery");
}

RecoveryStore::RecoveryStore(QString directory) : directory_(std::move(directory)) {}

RecoveryStore::~RecoveryStore() = default;

QString RecoveryStore::path(const QString& documentId, const char* suffix) const {
    return directory_ + QLatin1Char('/') + documentId + QLatin1String(suffix);
}

bool RecoveryStore::lock(const QString& documentId) {
    if (locks_.contains(documentId))
        return true;
    auto file = std::make_unique<QLockFile>(path(documentId, ".lock"));
    // The lock of a crashed instance names a process that no longer runs; Qt removes it.
    file->setStaleLockTime(0);
    if (!file->tryLock(0))
        return false;
    locks_.emplace(documentId, std::move(file));
    return true;
}

void RecoveryStore::release(const QString& documentId) { locks_.erase(documentId); }

bool RecoveryStore::write(const RecoverySnapshot& snapshot) {
    if (!QDir().mkpath(directory_))
        throw std::runtime_error("Recovery directory cannot be created");
    const QString id = QString::fromStdString(snapshot.documentId);
    if (!lock(id))
        return false;
    platform::AtomicFileStore::write(nativePath(path(id, ".volund")), snapshot.bytes);
    QStringList lines{
        QStringLiteral("format=%1").arg(metadataFormat), QStringLiteral("document=%1").arg(id),
        QStringLiteral("saved=%1").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),
        QStringLiteral("features=%1").arg(snapshot.features),
        QStringLiteral("file=%1").arg(snapshot.originalFile)};
    if (snapshot.savedFeatures)
        lines.append(QStringLiteral("savedFeatures=%1").arg(*snapshot.savedFeatures));
    // The draft goes first and apart; the metadata names it only once it is complete.
    if (snapshot.draft) {
        platform::AtomicFileStore::write(nativePath(path(id, ".draft")), *snapshot.draft);
        lines.append(QStringLiteral("draftFeature=%1").arg(snapshot.draftFeature));
        lines.append(QStringLiteral("draftEdits=%1").arg(snapshot.draftEdits ? 1 : 0));
    }
    platform::AtomicFileStore::write(
        nativePath(path(id, ".meta")),
        (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8().toStdString());
    if (!snapshot.draft)
        QFile::remove(path(id, ".draft"));
    return true;
}

void RecoveryStore::remove(const std::string& documentId) {
    const QString id = QString::fromStdString(documentId);
    if (!locks_.contains(id) && !lock(id))
        return; // Another running instance owns it.
    QFile::remove(path(id, ".meta"));
    QFile::remove(path(id, ".volund"));
    QFile::remove(path(id, ".draft"));
    release(id);
}

void RecoveryStore::removeStaleTemporaryFiles() const {
    const QDir directory(directory_);
    const auto now = QDateTime::currentDateTimeUtc();
    for (const QFileInfo& file : directory.entryInfoList({QStringLiteral(".voelund-save-*.tmp")},
                                                         QDir::Files | QDir::Hidden))
        if (file.lastModified().toUTC().secsTo(now) > staleTemporarySeconds)
            QFile::remove(file.absoluteFilePath());
}

std::vector<RecoveryEntry> RecoveryStore::candidates() {
    std::vector<RecoveryEntry> result;
    if (!QFileInfo::exists(directory_))
        return result;
    removeStaleTemporaryFiles();
    const QDir directory(directory_);
    QStringList ids;
    for (const QString& name :
         directory.entryList({QStringLiteral("*.meta"), QStringLiteral("*.volund")}, QDir::Files)) {
        const QString id = QFileInfo(name).completeBaseName();
        if (isDocumentId(id) && !ids.contains(id))
            ids.append(id);
    }
    for (const QString& id : ids) {
        // This instance's own entries (written or restored) and those another running
        // instance holds are not offered.
        if (locks_.contains(id) || !lock(id))
            continue;
        RecoveryEntry entry{id, {}, {}, 0, std::nullopt, false, true};
        QFile meta(path(id, ".meta"));
        if (meta.open(QIODevice::ReadOnly)) {
            std::map<QString, QString> values;
            for (const QByteArray& line : meta.readAll().split('\n')) {
                const auto separator = line.indexOf('=');
                if (separator > 0)
                    values[QString::fromUtf8(line.left(separator))] =
                        QString::fromUtf8(line.mid(separator + 1));
            }
            bool countOk = false;
            entry.features = values[QStringLiteral("features")].toULongLong(&countOk);
            entry.savedAt =
                QDateTime::fromString(values[QStringLiteral("saved")], Qt::ISODateWithMs);
            entry.originalFile = values[QStringLiteral("file")];
            if (values.contains(QStringLiteral("savedFeatures")))
                entry.savedFeatures = values[QStringLiteral("savedFeatures")].toULongLong();
            if (values.contains(QStringLiteral("draftFeature"))) {
                entry.draftFeature = values[QStringLiteral("draftFeature")].toULongLong();
                entry.draftEdits = values[QStringLiteral("draftEdits")] == QStringLiteral("1");
                entry.hasDraft =
                    entry.draftFeature != 0 && QFileInfo(path(id, ".draft")).size() > 0;
            }
            entry.damaged = values[QStringLiteral("format")] != QString::number(metadataFormat) ||
                            values[QStringLiteral("document")] != id || !countOk ||
                            !entry.savedAt.isValid() || QFileInfo(path(id, ".volund")).size() == 0;
        }
        if (!entry.damaged && !entry.originalFile.isEmpty()) {
            const QFileInfo original(entry.originalFile);
            entry.olderThanFile = original.exists() && original.lastModified() > entry.savedAt;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

std::string RecoveryStore::readDraft(const RecoveryEntry& entry) const {
    QFile data(path(entry.documentId, ".draft"));
    if (!entry.hasDraft || !data.open(QIODevice::ReadOnly))
        throw std::runtime_error("The sketch draft cannot be read");
    return data.readAll().toStdString();
}

std::string RecoveryStore::read(const RecoveryEntry& entry) const {
    QFile data(path(entry.documentId, ".volund"));
    if (entry.damaged || !data.open(QIODevice::ReadOnly))
        throw std::runtime_error("The recovery snapshot cannot be read");
    return data.readAll().toStdString();
}

} // namespace voelund::desktop
