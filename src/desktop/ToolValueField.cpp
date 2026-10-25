// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ToolValueField.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QCursor>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLocale>

#include <cmath>

namespace voelund::desktop {
namespace {
// A typed number, with or without its unit, in C or the user's locale.
std::optional<double> number(QString text) {
    for (const QString& unit : {QStringLiteral("mm"), QStringLiteral("°")})
        if (text.endsWith(unit))
            text = text.chopped(unit.size()).trimmed();
    bool ok = false;
    double value = QLocale::c().toDouble(text, &ok);
    if (!ok)
        value = QLocale().toDouble(text, &ok);
    return ok && std::isfinite(value) ? std::optional{value} : std::nullopt;
}
} // namespace

ToolValueField::ToolValueField(viewport::CadViewport& viewport)
    : QLineEdit(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("toolValueField"));
    setAlignment(Qt::AlignCenter);
    setFixedWidth(110);
    hide();
    connect(this, &QLineEdit::textEdited, this, [this](const QString& text) { typed(text); });
}

void ToolValueField::attach(QDoubleSpinBox* field, std::function<void()> accept) {
    if (field_)
        disconnect(field_, nullptr, this, nullptr);
    field_ = field;
    accept_ = std::move(accept);
    if (!field_) {
        hide();
        return;
    }
    setToolTip(field_->toolTip());
    connect(field_, &QDoubleSpinBox::valueChanged, this, [this] { follow(); });
    follow();
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    selectAll();
}

// A typed number goes into the panel's field at once; the preview follows.
void ToolValueField::typed(const QString& text) {
    const auto value = number(text);
    if (!field_ || !value)
        return;
    typing_ = true;
    field_->setValue(*value);
    typing_ = false;
    place();
}

// The panel's value (from the arrow or the panel) shows in the field, unless it is being typed.
void ToolValueField::follow() {
    if (!field_)
        return;
    if (!typing_)
        setText(QLocale().toString(field_->value(), 'f', field_->decimals()) +
                (field_->suffix().isEmpty() ? QString{} : field_->suffix()));
    place();
}

// Beside the arrow, else at the pointer; always inside the view.
void ToolValueField::place() {
    const QPoint anchor =
        viewport_.toolGripPoint().value_or(viewport_.mapFromGlobal(QCursor::pos()));
    QPoint at = anchor + QPoint(18, -height() / 2);
    at.setX(std::clamp(at.x(), 4, std::max(4, viewport_.width() - width() - 4)));
    at.setY(std::clamp(at.y(), 4, std::max(4, viewport_.height() - height() - 4)));
    move(at);
}

void ToolValueField::keyPressEvent(QKeyEvent* event) {
    // Enter confirms like OK when the panel's shortcut did not take it.
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && accept_) {
        typed(text());
        accept_();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

} // namespace voelund::desktop
