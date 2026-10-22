// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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
        // U14: centred in the 24 box (arrow and tail span y 5..20).
        QPainterPath path;
        path.moveTo(9, 5);
        path.lineTo(4, 10);
        path.lineTo(9, 15);
        painter.drawPath(path);
        QPainterPath tail;
        tail.moveTo(4, 10);
        tail.lineTo(15, 10);
        tail.arcTo(QRectF(10, 10, 10, 10), 90, -180);
        tail.lineTo(12, 20);
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
    case ShellIcon::sphere: // M27
        painter.drawEllipse(QRectF(4, 4, 16, 16));
        painter.drawArc(QRectF(4, 10, 16, 5), 180 * 16, 180 * 16);
        break;
    case ShellIcon::cone:
        painter.drawLine(QPointF(12, 3.5), QPointF(4.5, 17.5));
        painter.drawLine(QPointF(12, 3.5), QPointF(19.5, 17.5));
        painter.drawEllipse(QRectF(4.5, 15, 15, 5));
        break;
    case ShellIcon::torus:
        painter.drawEllipse(QRectF(2.5, 6.5, 19, 11));
        painter.drawEllipse(QRectF(8.5, 10, 7, 4));
        break;
    case ShellIcon::wedge: {
        QPainterPath path;
        path.moveTo(3.5, 19);
        path.lineTo(20.5, 19);
        path.lineTo(20.5, 15);
        path.lineTo(9, 5.5);
        path.lineTo(3.5, 5.5);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
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
    case ShellIcon::hole:
        // A block with a drilled hole seen from the side.
        painter.drawRect(QRectF(3.5, 8.5, 17, 11));
        painter.drawLine(QPointF(9.5, 8.5), QPointF(9.5, 19.5));
        painter.drawLine(QPointF(14.5, 8.5), QPointF(14.5, 19.5));
        painter.drawLine(QPointF(12, 2), QPointF(12, 6));
        break;
    case ShellIcon::datumPlane:
        // A plane in perspective with a dashed offset arrow.
        painter.drawLine(QPointF(3, 17), QPointF(15, 17));
        painter.drawLine(QPointF(15, 17), QPointF(21, 11));
        painter.drawLine(QPointF(21, 11), QPointF(9, 11));
        painter.drawLine(QPointF(9, 11), QPointF(3, 17));
        painter.drawLine(QPointF(12, 14), QPointF(12, 3));
        painter.drawLine(QPointF(9, 6), QPointF(12, 3));
        painter.drawLine(QPointF(15, 6), QPointF(12, 3));
        break;
    case ShellIcon::shell:
        // An open box seen from the side: walls around a hollow.
        painter.drawLine(QPointF(4, 6), QPointF(4, 20));
        painter.drawLine(QPointF(4, 20), QPointF(20, 20));
        painter.drawLine(QPointF(20, 20), QPointF(20, 6));
        painter.drawLine(QPointF(8, 6), QPointF(8, 16));
        painter.drawLine(QPointF(8, 16), QPointF(16, 16));
        painter.drawLine(QPointF(16, 16), QPointF(16, 6));
        break;
    case ShellIcon::revolve:
        // A profile turned about a dashed axis: an arc arrow around it.
        painter.drawLine(QPointF(12, 2), QPointF(12, 22));
        painter.drawArc(QRectF(4, 7, 16, 10), 200 * 16, 300 * 16);
        painter.drawLine(QPointF(17, 7.5), QPointF(20, 9));
        break;
    case ShellIcon::sweep: {
        // A circle travelling along a bent dashed path.
        painter.drawEllipse(QRectF(2.5, 14.5, 7, 7));
        QPainterPath path;
        path.moveTo(6, 14);
        path.cubicTo(6, 6, 12, 4, 21, 4);
        painter.drawPath(path);
        painter.drawEllipse(QRectF(18, 1.5, 3, 5));
        break;
    }
    case ShellIcon::helix: {
        // A coil: slanted loops along an upright axis.
        QPainterPath coil;
        coil.moveTo(5, 19);
        for (int turn = 0; turn < 3; ++turn) {
            const double y = 19 - 5.5 * turn;
            coil.cubicTo(5, y - 4, 19, y - 2, 19, y - 3);
            coil.cubicTo(19, y - 5, 5, y - 4, 5, y - 5.5);
        }
        painter.drawPath(coil);
        break;
    }
    case ShellIcon::draft:
        // A block whose sides lean in towards the top, over its neutral base line.
        painter.drawLine(QPointF(3, 20), QPointF(21, 20));
        painter.drawLine(QPointF(6, 20), QPointF(9, 5));
        painter.drawLine(QPointF(18, 20), QPointF(15, 5));
        painter.drawLine(QPointF(9, 5), QPointF(15, 5));
        break;
    case ShellIcon::mirror: {
        // A body and its mirror image on both sides of a dashed plane line.
        painter.drawPolyline(
            QPolygonF{QPointF(10, 19), QPointF(3, 19), QPointF(3, 5), QPointF(10, 12)});
        painter.drawPolyline(
            QPolygonF{QPointF(14, 19), QPointF(21, 19), QPointF(21, 5), QPointF(14, 12)});
        QPen plane = painter.pen();
        plane.setColor(accent);
        plane.setStyle(Qt::DashLine);
        painter.setPen(plane);
        painter.drawLine(QPointF(12, 2), QPointF(12, 22));
        break;
    }
    case ShellIcon::parameters: {
        // A small table: a name column and two rows, the "=" in the accent colour.
        painter.drawRoundedRect(QRectF(3.5, 4.5, 17, 15), 2, 2);
        painter.drawLine(QPointF(3.5, 9.5), QPointF(20.5, 9.5));
        painter.drawLine(QPointF(10.5, 4.5), QPointF(10.5, 19.5));
        painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(13.5, 12.5), QPointF(17.5, 12.5));
        painter.drawLine(QPointF(13.5, 16), QPointF(17.5, 16));
        break;
    }
    case ShellIcon::importFile:
        // An open tray with an arrow coming down into it, the arrow in the accent colour.
        painter.drawPolyline(QPolygonF{QPointF(3.5, 13.5), QPointF(3.5, 20.5), QPointF(20.5, 20.5),
                                       QPointF(20.5, 13.5)});
        painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(12, 3), QPointF(12, 15));
        painter.drawPolyline(QPolygonF{QPointF(7.5, 10.5), QPointF(12, 15), QPointF(16.5, 10.5)});
        break;
    case ShellIcon::measure:
        // A ruler with marks.
        painter.drawRect(QRectF(2.5, 8.5, 19, 7));
        for (const double x : {6.0, 10.0, 14.0, 18.0})
            painter.drawLine(QPointF(x, 8.5), QPointF(x, x == 10.0 || x == 18.0 ? 13 : 11));
        break;
    case ShellIcon::patternLinear:
        // Three blocks in a row, the copies drawn in the accent colour.
        painter.drawRect(QRectF(2.5, 8.5, 5, 7));
        painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(QRectF(9.5, 8.5, 5, 7));
        painter.drawRect(QRectF(16.5, 8.5, 5, 7));
        break;
    case ShellIcon::patternCircular:
        // Blocks around a centre point, the copies in the accent colour.
        painter.drawPoint(QPointF(12, 12));
        painter.drawRect(QRectF(9.5, 2.5, 5, 5));
        painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(QRectF(16.5, 9.5, 5, 5));
        painter.drawRect(QRectF(9.5, 16.5, 5, 5));
        painter.drawRect(QRectF(2.5, 9.5, 5, 5));
        break;
    case ShellIcon::loft:
        // A wide section below, a small one above, joined by straight sides.
        painter.drawEllipse(QRectF(3, 15, 18, 6));
        painter.drawEllipse(QRectF(8, 3, 8, 3.5));
        painter.drawLine(QPointF(3, 18), QPointF(8, 4.75));
        painter.drawLine(QPointF(21, 18), QPointF(16, 4.75));
        break;
    case ShellIcon::datumAxis:
        // A long axis through a small ring.
        painter.drawLine(QPointF(4, 20), QPointF(20, 4));
        painter.drawEllipse(QPointF(12, 12), 4, 4);
        break;
    case ShellIcon::combine:
        // Two overlapping bodies, the second one dashed: a Boolean of whole bodies.
        painter.drawRect(QRectF(3.5, 9.5, 11, 11));
        painter.drawRect(QRectF(9.5, 3.5, 11, 11));
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
