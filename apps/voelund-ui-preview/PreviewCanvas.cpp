#include "PreviewCanvas.hpp"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>

namespace voelund::preview {
namespace {

QPolygonF polygon(std::initializer_list<QPointF> points)
{
    QPolygonF result;
    for (const auto& point : points) {
        result << point;
    }
    return result;
}

void drawEndpoint(QPainter& painter, const QPointF& point, bool constrained)
{
    painter.setPen(QPen(constrained ? QColor("#67E4A6") : QColor("#F5B84B"), 1.4));
    painter.setBrush(QColor("#203038"));
    painter.drawEllipse(point, 4.5, 4.5);
}

} // namespace

PreviewCanvas::PreviewCanvas(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designCanvas"));
    setMinimumSize(580, 430);
    setMouseTracking(true);
}

void PreviewCanvas::setMode(Mode mode)
{
    mode_ = mode;
    sideSelected_ = false;
    update();
}

void PreviewCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintBackground(painter);
    if (mode_ == Mode::Solid) {
        paintSolid(painter);
    } else {
        paintSketch(painter);
    }
    paintViewCube(painter);
    paintNavigation(painter);
}

void PreviewCanvas::mousePressEvent(QMouseEvent* event)
{
    if (mode_ == Mode::Solid && event->button() == Qt::LeftButton) {
        sideSelected_ = !sideSelected_;
        emit selectionChanged(sideSelected_ ? QStringLiteral("Side face") : QStringLiteral("Body / Pad"));
        update();
    }
    QWidget::mousePressEvent(event);
}

void PreviewCanvas::paintBackground(QPainter& painter) const
{
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0, QColor("#303943"));
    background.setColorAt(1, QColor("#252D35"));
    painter.fillRect(rect(), background);

    painter.setPen(QPen(QColor(255, 255, 255, 8), 1));
    constexpr int grid = 36;
    for (int x = width() % grid; x < width(); x += grid) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = height() % grid; y < height(); y += grid) {
        painter.drawLine(0, y, width(), y);
    }
}

void PreviewCanvas::paintSolid(QPainter& painter) const
{
    const qreal scale = std::min(width() / 900.0, height() / 650.0);
    painter.save();
    painter.translate(width() * 0.5, height() * 0.51);
    painter.scale(scale, scale);

    QRadialGradient shadow(0, 178, 310);
    shadow.setColorAt(0, QColor(0, 0, 0, 100));
    shadow.setColorAt(1, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawEllipse(QRectF(-320, 85, 640, 190));

    const auto top = polygon({{-270, -25}, {-20, -165}, {290, 0}, {35, 145}});
    const auto left = polygon({{-270, -25}, {35, 145}, {35, 260}, {-270, 84}});
    const auto right = polygon({{35, 145}, {290, 0}, {290, 115}, {35, 260}});

    painter.setPen(QPen(QColor("#AFC0CD"), 1.2));
    painter.setBrush(QColor("#CBD6DE")); painter.drawPolygon(top);
    painter.setBrush(sideSelected_ ? QColor("#219DCC") : QColor("#8297A5")); painter.drawPolygon(left);
    painter.setBrush(QColor("#A5B5C0")); painter.drawPolygon(right);

    const auto towerTop = polygon({{-80, -90}, {15, -144}, {120, -86}, {23, -31}});
    const auto towerLeft = polygon({{-80, -90}, {23, -31}, {23, 85}, {-80, 26}});
    const auto towerRight = polygon({{23, -31}, {120, -86}, {120, 30}, {23, 85}});
    painter.setBrush(QColor("#E2E9EE")); painter.drawPolygon(towerTop);
    painter.setBrush(QColor("#9EB0BC")); painter.drawPolygon(towerLeft);
    painter.setBrush(QColor("#BBC8D1")); painter.drawPolygon(towerRight);

    if (sideSelected_) {
        painter.setPen(QPen(QColor("#65D7FF"), 2.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(left);
    } else {
        painter.setPen(QPen(QColor("#54C7F4"), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(top);
    }

    painter.restore();

    painter.setPen(QColor("#DCE6ED"));
    QFont caption = painter.font(); caption.setPointSizeF(10.5); caption.setWeight(QFont::DemiBold); painter.setFont(caption);
    painter.drawText(QRect(24, 22, 360, 25), Qt::AlignLeft, sideSelected_ ? QStringLiteral("SIDE FACE SELECTED") : QStringLiteral("BODY / PAD"));
    painter.setPen(QColor("#91A0AD"));
    QFont small = painter.font(); small.setPointSizeF(9); small.setWeight(QFont::Normal); painter.setFont(small);
    painter.drawText(QRect(24, 44, 400, 24), Qt::AlignLeft, QStringLiteral("Click canvas to change selection"));
}

void PreviewCanvas::paintSketch(QPainter& painter) const
{
    const qreal canvasWidth = std::min(width() * 0.72, 720.0);
    const qreal canvasHeight = std::min(height() * 0.72, 500.0);
    const QRectF plane((width() - canvasWidth) / 2.0, (height() - canvasHeight) / 2.0, canvasWidth, canvasHeight);

    painter.setPen(QPen(QColor("#58BDE8"), 1.2));
    painter.setBrush(QColor("#F3F5F3"));
    painter.drawRoundedRect(plane, 2, 2);

    painter.save();
    painter.setClipRect(plane);
    painter.setPen(QPen(QColor(45, 65, 72, 18), 1));
    for (qreal x = plane.left(); x <= plane.right(); x += 25) painter.drawLine(QPointF(x, plane.top()), QPointF(x, plane.bottom()));
    for (qreal y = plane.top(); y <= plane.bottom(); y += 25) painter.drawLine(QPointF(plane.left(), y), QPointF(plane.right(), y));
    painter.setPen(QPen(QColor("#D65757"), 1.3)); painter.drawLine(QPointF(plane.left(), plane.center().y()), QPointF(plane.right(), plane.center().y()));
    painter.setPen(QPen(QColor("#4AAE69"), 1.3)); painter.drawLine(QPointF(plane.center().x(), plane.top()), QPointF(plane.center().x(), plane.bottom()));
    painter.restore();

    const QPointF c = plane.center();
    const QPointF a(c.x() - 155, c.y() + 80);
    const QPointF b(c.x() + 120, c.y() + 80);
    const QPointF d(c.x() + 120, c.y() - 70);
    const QPointF e(c.x() - 155, c.y() - 70);
    painter.setPen(QPen(QColor("#235D72"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(polygon({a, b, d, e}));
    painter.drawEllipse(QPointF(c.x() - 15, c.y() + 5), 48, 48);
    for (const QPointF& point : {a, b, d, e}) drawEndpoint(painter, point, true);

    painter.setPen(QPen(QColor("#3187A8"), 1));
    painter.drawLine(QPointF(a.x(), a.y() + 27), QPointF(b.x(), b.y() + 27));
    painter.drawLine(QPointF(a.x(), a.y() + 20), QPointF(a.x(), a.y() + 34));
    painter.drawLine(QPointF(b.x(), b.y() + 20), QPointF(b.x(), b.y() + 34));
    painter.setBrush(QColor("#F3F5F3")); painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(c.x() - 38, a.y() + 17, 76, 22), 4, 4);
    painter.setPen(QColor("#235D72")); painter.drawText(QRectF(c.x() - 38, a.y() + 17, 76, 22), Qt::AlignCenter, QStringLiteral("55.00 mm"));

    painter.setPen(QColor("#DCE6ED"));
    QFont caption = painter.font(); caption.setPointSizeF(10.5); caption.setWeight(QFont::DemiBold); painter.setFont(caption);
    painter.drawText(QRect(24, 22, 340, 25), Qt::AlignLeft, QStringLiteral("SKETCH 01  ·  TOP FACE"));
    painter.setPen(QColor("#F0B44D"));
    QFont small = painter.font(); small.setPointSizeF(9); small.setWeight(QFont::Normal); painter.setFont(small);
    painter.drawText(QRect(24, 44, 400, 24), Qt::AlignLeft, QStringLiteral("Under-constrained  ·  4 DOF"));
}

void PreviewCanvas::paintViewCube(QPainter& painter) const
{
    const QPointF center(width() - 70.0, 60.0);
    const auto top = polygon({{center.x() - 27, center.y() - 12}, {center.x(), center.y() - 28}, {center.x() + 27, center.y() - 12}, {center.x(), center.y() + 4}});
    const auto left = polygon({{center.x() - 27, center.y() - 12}, {center.x(), center.y() + 4}, {center.x(), center.y() + 34}, {center.x() - 27, center.y() + 18}});
    const auto right = polygon({{center.x(), center.y() + 4}, {center.x() + 27, center.y() - 12}, {center.x() + 27, center.y() + 18}, {center.x(), center.y() + 34}});
    painter.setPen(QPen(QColor("#788895"), 1));
    painter.setBrush(QColor("#D7E0E6")); painter.drawPolygon(top);
    painter.setBrush(QColor("#718593")); painter.drawPolygon(left);
    painter.setBrush(QColor("#9AACB8")); painter.drawPolygon(right);
    painter.setPen(QColor("#27323A"));
    QFont font = painter.font(); font.setPointSizeF(7.5); font.setWeight(QFont::DemiBold); painter.setFont(font);
    painter.drawText(QRectF(center.x() - 24, center.y() - 23, 48, 20), Qt::AlignCenter, QStringLiteral("TOP"));
}

void PreviewCanvas::paintNavigation(QPainter& painter) const
{
    const QRectF bar(width() / 2.0 - 114, height() - 44, 228, 30);
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.setBrush(QColor(18, 23, 28, 205));
    painter.drawRoundedRect(bar, 7, 7);
    painter.setPen(QColor("#A9B5C0"));
    QFont font = painter.font(); font.setPointSizeF(9); painter.setFont(font);
    painter.drawText(bar, Qt::AlignCenter, QStringLiteral("◉  Orbit     ▣  Pan     ⊕  Zoom     ⌂  Fit"));
}

} // namespace voelund::preview
