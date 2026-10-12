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
    case ShellIcon::search:
        painter.drawEllipse(QPointF(10.5, 10.5), 6, 6);
        painter.drawLine(QPointF(15, 15), QPointF(20, 20));
        break;
    case ShellIcon::rail:
        painter.drawRoundedRect(QRectF(3, 4, 6, 16), 1.5, 1.5);
        painter.drawLine(QPointF(12, 7), QPointF(21, 7));
        painter.drawLine(QPointF(12, 12), QPointF(21, 12));
        painter.drawLine(QPointF(12, 17), QPointF(21, 17));
        break;
    case ShellIcon::box: {
        QPainterPath path;
        path.moveTo(12, 3);
        path.lineTo(20, 7.5);
        path.lineTo(20, 16.5);
        path.lineTo(12, 21);
        path.lineTo(4, 16.5);
        path.lineTo(4, 7.5);
        path.closeSubpath();
        path.moveTo(4, 7.5);
        path.lineTo(12, 12);
        path.lineTo(20, 7.5);
        path.moveTo(12, 12);
        path.lineTo(12, 21);
        painter.drawPath(path);
        break;
    }
    case ShellIcon::cylinder:
        painter.drawEllipse(QRectF(5, 3.5, 14, 5));
        painter.drawLine(QPointF(5, 6), QPointF(5, 18));
        painter.drawLine(QPointF(19, 6), QPointF(19, 18));
        painter.drawArc(QRectF(5, 15.5, 14, 5), 180 * 16, 180 * 16);
        break;
    case ShellIcon::sketch:
        painter.drawLine(QPointF(4, 20), QPointF(20, 20));
        painter.drawLine(QPointF(6, 17), QPointF(15, 8));
        painter.drawLine(QPointF(15, 8), QPointF(18, 11));
        painter.drawLine(QPointF(18, 11), QPointF(9, 20));
        break;
    case ShellIcon::extrude:
        painter.drawLine(QPointF(5, 20), QPointF(19, 20));
        painter.drawLine(QPointF(12, 17), QPointF(12, 5));
        painter.drawLine(QPointF(8, 9), QPointF(12, 5));
        painter.drawLine(QPointF(16, 9), QPointF(12, 5));
        break;
    case ShellIcon::transform:
        painter.drawLine(QPointF(12, 3), QPointF(12, 21));
        painter.drawLine(QPointF(3, 12), QPointF(21, 12));
        painter.drawLine(QPointF(9, 6), QPointF(12, 3));
        painter.drawLine(QPointF(15, 6), QPointF(12, 3));
        painter.drawLine(QPointF(18, 9), QPointF(21, 12));
        painter.drawLine(QPointF(18, 15), QPointF(21, 12));
        break;
    case ShellIcon::pushPull:
        // A face with an arrow out of it and one into it (T04c).
        painter.drawLine(QPointF(3, 15), QPointF(15, 15));
        painter.drawLine(QPointF(15, 15), QPointF(21, 9));
        painter.drawLine(QPointF(21, 9), QPointF(9, 9));
        painter.drawLine(QPointF(9, 9), QPointF(3, 15));
        painter.drawLine(QPointF(12, 12), QPointF(12, 2));
        painter.drawLine(QPointF(9, 5), QPointF(12, 2));
        painter.drawLine(QPointF(15, 5), QPointF(12, 2));
        painter.drawLine(QPointF(12, 15), QPointF(12, 22));
        break;
    case ShellIcon::fillet:
        // A body corner rounded by a quarter circle.
        painter.drawLine(QPointF(4, 20), QPointF(4, 12));
        painter.drawArc(QRectF(4, 4, 16, 16), 90 * 16, 90 * 16);
        painter.drawLine(QPointF(12, 4), QPointF(20, 4));
        break;
    case ShellIcon::chamfer:
        // A body corner cut by a straight bevel.
        painter.drawLine(QPointF(4, 20), QPointF(4, 11));
        painter.drawLine(QPointF(4, 11), QPointF(11, 4));
        painter.drawLine(QPointF(11, 4), QPointF(20, 4));
        break;
    case ShellIcon::editSketch:
        painter.drawRect(QRectF(3.5, 6.5, 12, 12));
        painter.drawLine(QPointF(11, 15), QPointF(20, 6));
        painter.drawLine(QPointF(18, 4), QPointF(22, 8));
        break;
    case ShellIcon::repair:
        painter.drawLine(QPointF(5, 19), QPointF(13, 11));
        painter.drawEllipse(QPointF(16, 8), 4, 4);
        break;
    case ShellIcon::eye:
    case ShellIcon::eyeOff: {
        QPainterPath lid;
        lid.moveTo(2.5, 12);
        lid.quadTo(12, 3, 21.5, 12);
        lid.quadTo(12, 21, 2.5, 12);
        painter.drawPath(lid);
        painter.drawEllipse(QPointF(12, 12), 3, 3);
        if (icon == ShellIcon::eyeOff)
            painter.drawLine(QPointF(4, 4), QPointF(20, 20));
        break;
    }
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
