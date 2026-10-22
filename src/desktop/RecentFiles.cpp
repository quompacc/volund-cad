// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "RecentFiles.hpp"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

#include <utility>

namespace voelund::desktop {
namespace {

const QString key = QStringLiteral("document/recent");

void store(const QStringList& files) { QSettings().setValue(key, files); }

} // namespace

RecentFiles::RecentFiles(QMenu& fileMenu, QAction* before, QWidget& parent,
                         std::function<void(const QString&)> open)
    : parent_(parent), open_(std::move(open)) {
    menu_ = new QMenu(QObject::tr("&Zuletzt geöffnet"), &fileMenu);
    menu_->setObjectName(QStringLiteral("recentMenu"));
    fileMenu.insertMenu(before, menu_);
    QObject::connect(menu_, &QMenu::aboutToShow, menu_, [this] { fill(); });
}

QStringList RecentFiles::files() { return QSettings().value(key).toStringList(); }

void RecentFiles::remember(const QString& file) {
    if (file.isEmpty())
        return;
    const QString path = QFileInfo(file).absoluteFilePath();
    QStringList list = files();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > maximum)
        list.removeLast();
    store(list);
}

void RecentFiles::fill() {
    menu_->clear();
    const QStringList list = files();
    if (list.isEmpty()) {
        QAction* none = menu_->addAction(QObject::tr("Keine Dokumente"));
        none->setProperty("commandId", QStringLiteral("document.recent_none"));
        none->setEnabled(false);
        return;
    }
    for (const QString& file : list) {
        QAction* entry = menu_->addAction(QFileInfo(file).fileName());
        entry->setToolTip(QDir::toNativeSeparators(file));
        entry->setStatusTip(QDir::toNativeSeparators(file));
        entry->setProperty("commandId", QStringLiteral("document.open_recent"));
        entry->setData(file);
        QObject::connect(entry, &QAction::triggered, menu_, [this, file] { choose(file); });
    }
    menu_->addSeparator();
    QAction* clear = menu_->addAction(QObject::tr("Liste leeren"));
    clear->setProperty("commandId", QStringLiteral("document.clear_recent"));
    QObject::connect(clear, &QAction::triggered, menu_, [] { store({}); });
}

void RecentFiles::choose(const QString& file) {
    if (!QFileInfo::exists(file)) {
        QStringList list = files();
        list.removeAll(file);
        store(list);
        QMessageBox::information(
            &parent_, QObject::tr("Dokument nicht gefunden"),
            QObject::tr("„%1“ gibt es nicht mehr; es wurde aus der Liste entfernt.")
                .arg(QDir::toNativeSeparators(file)));
        return;
    }
    open_(file);
}

} // namespace voelund::desktop
