// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QFrame>

#include <functional>

class QLabel;

namespace voelund::desktop {

// U10 (user decision 2026-10-07): a band at the top of the viewport while the model has
// errors and the view shows the last valid geometry; "Zum Fehler" selects the failed feature.
class StaleBanner final : public QFrame {
  public:
    explicit StaleBanner(QWidget& viewport);
    void setOpenErrorHandler(std::function<void()> handler);
    // Hidden for an empty text.
    void showText(const QString& text);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void place();

    QLabel* text_{nullptr};
    std::function<void()> openError_;
};

} // namespace voelund::desktop
