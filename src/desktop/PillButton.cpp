// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "PillButton.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QStyle>

namespace voelund::desktop {

PillButton::PillButton(QWidget* parent) : QToolButton(parent) {}

void PillButton::setChevron(bool chevron) {
    chevron_ = chevron;
    setProperty("chevron", chevron);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void PillButton::setChevronInset(double inset) {
    inset_ = inset;
    update();
}

void PillButton::paintEvent(QPaintEvent* event) {
    QToolButton::paintEvent(event);
    if (!chevron_ && menu() == nullptr)
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor ink = palette().color(QPalette::ButtonText);
    if (!isEnabled())
        ink.setAlphaF(0.35F);
    painter.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF center(width() - inset_, height() / 2.0 + 1.0);
    QPainterPath chevron;
    chevron.moveTo(center + QPointF(-3.0, -1.75));
    chevron.lineTo(center + QPointF(0, 1.25));
    chevron.lineTo(center + QPointF(3.0, -1.75));
    painter.drawPath(chevron);
}

} // namespace voelund::desktop
