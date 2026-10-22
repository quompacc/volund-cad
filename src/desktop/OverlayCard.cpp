// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OverlayCard.hpp"

#include <QEvent>

#include <utility>

namespace voelund::desktop {

OverlayCard::OverlayCard(QWidget& host, const QString& objectName, Placement placement)
    : QFrame(&host), host_(host), placement_(std::move(placement)) {
    setObjectName(objectName);
    setProperty("overlayCard", true);
    setAttribute(Qt::WA_StyledBackground);
    host.installEventFilter(this);
}

void OverlayCard::setPlacement(Placement placement) {
    placement_ = std::move(placement);
    place();
}

void OverlayCard::place() {
    if (!placement_ || placing_)
        return;
    placing_ = true;
    setGeometry(placement_(host_));
    raise();
    placing_ = false;
}

bool OverlayCard::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &host_ && event->type() == QEvent::Resize)
        place();
    return QFrame::eventFilter(watched, event);
}

bool OverlayCard::event(QEvent* event) {
    const bool result = QFrame::event(event);
    if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::StyleChange ||
        event->type() == QEvent::Show)
        place();
    return result;
}

} // namespace voelund::desktop
