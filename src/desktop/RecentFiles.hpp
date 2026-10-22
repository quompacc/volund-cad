// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QAction;
class QMenu;
class QWidget;

namespace voelund::desktop {

// N02: "Zuletzt geöffnet" in the file menu - the last documents this user opened or saved,
// newest first, kept in the user's settings. The entries are built when the submenu opens; a
// file that is gone is named and taken off the list when chosen.
class RecentFiles final {
  public:
    static constexpr int maximum = 10;
    RecentFiles(QMenu& fileMenu, QAction* before, QWidget& parent,
                std::function<void(const QString&)> open);

    void remember(const QString& file);
    [[nodiscard]] static QStringList files();

  private:
    void fill();
    void choose(const QString& file);

    QMenu* menu_{nullptr};
    QWidget& parent_;
    std::function<void(const QString&)> open_;
};

} // namespace voelund::desktop
