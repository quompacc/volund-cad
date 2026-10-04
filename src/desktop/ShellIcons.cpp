#include "ShellIcons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cmath>
#include <numbers>

namespace voelund::desktop {
namespace {

void draw(QPainter& painter, ShellIcon icon, const QColor& ink, const QColor& accent) {
    QPen pen(ink, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (icon) {
    case ShellIcon::brand: {
        // Anvil with a spark: the forge of Völund.
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        QPainterPath anvil;
        anvil.moveTo(3.5, 8.5);
        anvil.lineTo(16, 8.5);
        anvil.lineTo(20.5, 11);
        anvil.lineTo(20.5, 12.5);
        anvil.lineTo(7.5, 12.5);
        anvil.closeSubpath();
        anvil.addRect(QRectF(9, 12.5, 7, 2.5));
        anvil.addRect(QRectF(7.5, 15, 10, 3));
        painter.drawPath(anvil);
        painter.setBrush(accent);
        painter.drawEllipse(QPointF(18.5, 5), 1.8, 1.8);
        break;
    }
    case ShellIcon::menu:
        painter.drawLine(QPointF(4, 7), QPointF(20, 7));
        painter.drawLine(QPointF(4, 12), QPointF(20, 12));
        painter.drawLine(QPointF(4, 17), QPointF(20, 17));
        break;
    case ShellIcon::undo:
    case ShellIcon::redo: {
        if (icon == ShellIcon::redo) {
            painter.translate(24, 0);
            painter.scale(-1, 1);
        }
        QPainterPath path;
        path.moveTo(9, 7);
        path.lineTo(4, 12);
        path.lineTo(9, 17);
        painter.drawPath(path);
        QPainterPath tail;
        tail.moveTo(4, 12);
        tail.lineTo(15, 12);
        tail.arcTo(QRectF(10, 12, 10, 10), 90, -180);
        tail.lineTo(12, 22);
        painter.drawPath(tail);
        break;
    }
    case ShellIcon::sun:
        painter.drawEllipse(QPointF(12, 12), 4, 4);
        for (int ray = 0; ray < 8; ++ray) {
            const double angle = ray * std::numbers::pi / 4;
            painter.drawLine(QPointF(12 + 7 * std::cos(angle), 12 + 7 * std::sin(angle)),
                             QPointF(12 + 9.5 * std::cos(angle), 12 + 9.5 * std::sin(angle)));
        }
        break;
    case ShellIcon::moon: {
        QPainterPath outer;
        outer.addEllipse(QPointF(12, 12), 8, 8);
        QPainterPath bite;
        bite.addEllipse(QPointF(16, 8.5), 6.5, 6.5);
        painter.drawPath(outer.subtracted(bite));
        break;
    }
    }
}

QPixmap render(ShellIcon icon, const QColor& ink, const QColor& accent, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    draw(painter, icon, ink, accent);
    return pixmap;
}

} // namespace

QIcon shellIcon(ShellIcon icon, const QColor& ink, const QColor& accent) {
    QIcon result;
    for (const int size : {18, 20, 24, 36, 40, 48})
        result.addPixmap(render(icon, ink, accent, size));
    QColor disabled = ink;
    disabled.setAlphaF(0.35F);
    for (const int size : {18, 20, 24, 36, 40, 48})
        result.addPixmap(render(icon, disabled, disabled, size), QIcon::Disabled);
    return result;
}

} // namespace voelund::desktop
