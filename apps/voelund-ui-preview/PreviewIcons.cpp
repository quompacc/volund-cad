#include "PreviewIcons.hpp"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace voelund::preview {
namespace {

void drawIcon(QPainter& painter, Icon icon)
{
    const auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    const auto box = [&](qreal x, qreal y, qreal w, qreal h) {
        painter.drawRoundedRect(QRectF(x, y, w, h), 1.4, 1.4);
    };

    switch (icon) {
    case Icon::Logo: {
        QPainterPath path;
        path.moveTo(4, 5);
        path.lineTo(9, 19);
        path.lineTo(12, 11);
        path.lineTo(15, 19);
        path.lineTo(20, 5);
        painter.drawPath(path);
        break;
    }
    case Icon::Sketch:
        box(4, 5, 16, 14); line(7, 16, 11, 10); line(11, 10, 17, 14);
        painter.drawEllipse(QPointF(11, 10), 1.3, 1.3); break;
    case Icon::Extrude:
        box(4, 11, 12, 9); box(8, 5, 12, 9); line(4, 11, 8, 5); line(16, 11, 20, 5); break;
    case Icon::Revolve:
        painter.drawArc(QRectF(4, 4, 16, 16), 35 * 16, 285 * 16);
        line(18, 4, 20, 9); line(18, 4, 14, 5); line(12, 7, 12, 18); break;
    case Icon::Hole:
        painter.drawEllipse(QRectF(4, 5, 16, 8)); painter.drawEllipse(QRectF(8, 7, 8, 4));
        line(4, 9, 7, 18); line(20, 9, 17, 18); painter.drawArc(QRectF(7, 14, 10, 6), 180 * 16, 180 * 16); break;
    case Icon::Fillet:
        line(5, 19, 5, 12); painter.drawArc(QRectF(5, 5, 14, 14), 90 * 16, 90 * 16); line(12, 5, 19, 5); break;
    case Icon::Chamfer:
        line(5, 19, 5, 11); line(5, 11, 11, 5); line(11, 5, 19, 5); break;
    case Icon::Shell:
        box(4, 4, 16, 16); box(8, 8, 8, 8); line(16, 8, 20, 4); break;
    case Icon::Joint:
        painter.drawEllipse(QPointF(8, 12), 4, 4); painter.drawEllipse(QPointF(16, 12), 4, 4); line(11, 12, 13, 12); break;
    case Icon::Line:
        line(5, 18, 19, 6); painter.drawEllipse(QPointF(5, 18), 1.5, 1.5); painter.drawEllipse(QPointF(19, 6), 1.5, 1.5); break;
    case Icon::Rectangle:
        box(4, 6, 16, 12); break;
    case Icon::Circle:
        painter.drawEllipse(QPointF(12, 12), 8, 8); painter.drawPoint(QPointF(12, 12)); break;
    case Icon::Arc:
        painter.drawArc(QRectF(4, 5, 16, 16), 10 * 16, 160 * 16); line(4, 13, 7, 13); line(17, 7, 18, 4); break;
    case Icon::Constraint:
        line(4, 8, 20, 8); line(4, 16, 20, 16); line(8, 5, 8, 19); line(16, 5, 16, 19); break;
    case Icon::Dimension:
        line(4, 7, 4, 18); line(20, 7, 20, 18); line(5, 13, 19, 13); line(5, 13, 8, 11); line(5, 13, 8, 15); line(19, 13, 16, 11); line(19, 13, 16, 15); break;
    case Icon::Trim:
        painter.drawEllipse(QPointF(8, 8), 3, 3); painter.drawEllipse(QPointF(8, 16), 3, 3);
        line(10, 10, 20, 19); line(10, 14, 20, 5); break;
    case Icon::Offset:
        painter.drawArc(QRectF(4, 5, 13, 13), 0, 270 * 16); painter.drawArc(QRectF(8, 8, 12, 12), 0, 270 * 16); break;
    case Icon::Finish:
        line(4, 12, 9, 17); line(9, 17, 20, 6); break;
    case Icon::Search:
        painter.drawEllipse(QPointF(10, 10), 5, 5); line(14, 14, 20, 20); break;
    case Icon::Help:
        painter.drawEllipse(QPointF(12, 12), 9, 9); painter.drawText(QRectF(4, 3, 16, 18), Qt::AlignCenter, QStringLiteral("?")); break;
    case Icon::Settings:
        painter.drawEllipse(QPointF(12, 12), 7, 7); painter.drawEllipse(QPointF(12, 12), 2, 2);
        for (int i = 0; i < 8; ++i) { painter.save(); painter.translate(12, 12); painter.rotate(i * 45); line(0, -7, 0, -10); painter.restore(); } break;
    case Icon::Eye:
        painter.drawArc(QRectF(3, 7, 18, 10), 0, 180 * 16); painter.drawArc(QRectF(3, 7, 18, 10), 180 * 16, 180 * 16); painter.drawEllipse(QPointF(12, 12), 2.3, 2.3); break;
    case Icon::Chevron:
        line(7, 9, 12, 14); line(12, 14, 17, 9); break;
    case Icon::Select: {
        QPainterPath path; path.moveTo(5, 3); path.lineTo(18, 14); path.lineTo(12, 15); path.lineTo(9, 21); path.closeSubpath(); painter.drawPath(path); break;
    }
    }
}

} // namespace

QIcon makeIcon(Icon icon, const QColor& color)
{
    QIcon result;
    for (const int extent : {24, 48}) {
        QPixmap pixmap(extent, extent);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(static_cast<qreal>(extent) / 24.0, static_cast<qreal>(extent) / 24.0);
        QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        drawIcon(painter, icon);
        result.addPixmap(pixmap);
    }
    return result;
}

} // namespace voelund::preview
