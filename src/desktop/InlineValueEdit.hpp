// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QLineEdit>

#include <functional>

namespace voelund::desktop {

// D02 (as in Fusion and SolidCraft): a value typed where the work is, not in a dialog. The
// field opens at a point of the viewport with the current value selected, so typing replaces
// it; Enter reports the text, Escape or a click elsewhere closes it without a change (each
// reported on its own, a click elsewhere may continue the work there). Expression
// text such as "=Breite" is reported as typed; the caller decides what it means.
class InlineValueEdit final : public QLineEdit {
  public:
    struct Handlers final {
        std::function<void(const QString&)> committed;
        std::function<void()> cancelled; // Escape
        std::function<void()> left;      // a click elsewhere
    };

    explicit InlineValueEdit(QWidget& host);

    // Opens at `position` (host coordinates) with `text` selected; `hint` is the tooltip.
    void open(QPoint position, const QString& text, const QString& hint,
              const QStringList& completions, Handlers handlers);
    void dismiss();

  protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

  private:
    enum class Ending { commit, escape, leave };
    void finish(Ending ending);

    QWidget& host_;
    Handlers handlers_;
    bool finishing_{false};
};

} // namespace voelund::desktop
