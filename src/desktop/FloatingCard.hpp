// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QFrame>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace voelund::desktop {

// U05: a floating card at the right edge of its host (the viewport), below the view cube
// (ADR 0012). Its content scrolls when the host is too short. The tool panel and the
// feature properties share this slot; the shell shows at most one of them.
class FloatingCard : public QFrame {
  public:
    FloatingCard(QWidget& host, const QString& objectName);

    void setTitle(const QString& title);
    // Re-measures the card after its content changed visibility.
    void refit() { place(); }
    [[nodiscard]] QVBoxLayout& body() const noexcept { return *body_; }
    // Always visible below the scrolling body, e.g. OK and Cancel.
    [[nodiscard]] QVBoxLayout& footer() const noexcept { return *footer_; }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

  private:
    void place();

    QWidget& host_;
    QLabel* title_{nullptr};
    QScrollArea* scroll_{nullptr};
    QWidget* content_{nullptr};
    QVBoxLayout* body_{nullptr};
    QVBoxLayout* footer_{nullptr};
};

} // namespace voelund::desktop
