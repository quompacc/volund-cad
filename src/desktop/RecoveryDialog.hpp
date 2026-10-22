// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "RecoveryStore.hpp"

#include <QDialog>

#include <functional>
#include <vector>

class QListWidget;
class QPushButton;

// L05 (user decision 2026-10-07): a dialog at start lists the unsaved work found, one row per
// document with file, time and feature counts against the saved state. Restore opens the
// selected entry as an unsaved document, Discard deletes it, Later keeps everything for the
// next start. A damaged entry can only be discarded.
namespace voelund::desktop {

class RecoveryDialog final : public QDialog {
  public:
    using Handler = std::function<bool(const RecoveryEntry&)>;
    RecoveryDialog(std::vector<RecoveryEntry> entries, Handler restore, Handler discard,
                   QWidget* parent = nullptr);
    [[nodiscard]] static QString describe(const RecoveryEntry& entry);

  private:
    void syncButtons();
    void fill();
    [[nodiscard]] const RecoveryEntry* selected() const;

    std::vector<RecoveryEntry> entries_;
    Handler restore_;
    Handler discard_;
    QListWidget* list_{nullptr};
    QPushButton* restoreButton_{nullptr};
    QPushButton* discardButton_{nullptr};
};

} // namespace voelund::desktop
