// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "InlineValueEdit.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>

#include <algorithm>

namespace voelund::desktop {

InlineValueEdit::InlineValueEdit(QWidget& host) : QLineEdit(&host), host_(host) {
    setObjectName(QStringLiteral("inlineValueEdit"));
    setFixedWidth(150);
    setAlignment(Qt::AlignCenter);
    setStyleSheet(QStringLiteral("QLineEdit { border: 2px solid palette(highlight); "
                                 "border-radius: 6px; padding: 2px 6px; }"));
    hide();
}

void InlineValueEdit::open(QPoint position, const QString& text, const QString& hint,
                           const QStringList& completions, Handlers handlers) {
    handlers_ = std::move(handlers);
    setText(text);
    setToolTip(hint);
    setPlaceholderText(hint);
    auto* completer = new QCompleter(completions, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(completer);
    // Beside the pointer, inside the host.
    const QPoint wanted = position + QPoint(14, 14);
    move(std::clamp(wanted.x(), 0, std::max(0, host_.width() - width())),
         std::clamp(wanted.y(), 0, std::max(0, host_.height() - height())));
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    selectAll();
}

void InlineValueEdit::dismiss() {
    handlers_ = {};
    hide();
}

bool InlineValueEdit::event(QEvent* event) {
    // Enter and Escape belong to the field, not to the window's shortcuts (Enter would accept
    // the whole sketch, Escape step back in it).
    if (event->type() == QEvent::ShortcutOverride) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape) {
            event->accept();
            return true;
        }
    }
    return QLineEdit::event(event);
}

void InlineValueEdit::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(Ending::commit);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        finish(Ending::escape);
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void InlineValueEdit::focusOutEvent(QFocusEvent* event) {
    QLineEdit::focusOutEvent(event);
    // The completer's popup takes the focus for a moment; that is no click elsewhere.
    if (isVisible() && (completer() == nullptr || !completer()->popup()->isVisible()))
        finish(Ending::leave);
}

void InlineValueEdit::finish(Ending ending) {
    if (finishing_ || isHidden())
        return;
    finishing_ = true;
    auto handlers = std::move(handlers_);
    handlers_ = {};
    const QString value = text().trimmed();
    hide();
    if (ending != Ending::leave)
        host_.setFocus(Qt::OtherFocusReason);
    if (ending == Ending::commit && handlers.committed)
        handlers.committed(value);
    else if (ending == Ending::escape && handlers.cancelled)
        handlers.cancelled();
    else if (ending == Ending::leave && handlers.left)
        handlers.left();
    finishing_ = false;
}

} // namespace voelund::desktop
