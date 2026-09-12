#include "ViewCube.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

#include <utility>

namespace voelund::preview {

ViewCube::ViewCube(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(126, 142);
    setMouseTracking(true);
    setToolTip(QStringLiteral("Choose a standard camera view"));
}

void ViewCube::setCameraHandler(std::function<void(viewport::CameraView)> handler)
{
    cameraHandler_ = std::move(handler);
}

void ViewCube::setFitHandler(std::function<void()> handler)
{
    fitHandler_ = std::move(handler);
}

void ViewCube::setProjectionHandler(std::function<void(bool)> handler)
{
    projectionHandler_ = std::move(handler);
}

void ViewCube::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(8, 13, 9, 220));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 7, 7);

    const QPolygonF top{{18, 34}, {62, 10}, {106, 34}, {62, 58}};
    const QPolygonF front{{18, 34}, {62, 58}, {62, 100}, {18, 76}};
    const QPolygonF right{{62, 58}, {106, 34}, {106, 76}, {62, 100}};
    const auto faceColor = [this](Region region, const QColor& normal) {
        return hovered_ == region ? QColor("#C99A27") : normal;
    };
    painter.setPen(QPen(QColor("#A68125"), 1.2));
    painter.setBrush(faceColor(Region::top, QColor("#D8D5C8"))); painter.drawPolygon(top);
    painter.setBrush(faceColor(Region::front, QColor("#777E73"))); painter.drawPolygon(front);
    painter.setBrush(faceColor(Region::right, QColor("#9BA095"))); painter.drawPolygon(right);

    QFont faceFont = painter.font(); faceFont.setPointSizeF(7.5); faceFont.setWeight(QFont::DemiBold); painter.setFont(faceFont);
    painter.setPen(QColor("#20271F"));
    painter.drawText(QRectF(28, 23, 68, 24), Qt::AlignCenter, QStringLiteral("TOP"));
    painter.setPen(QColor("#EEE9DC"));
    painter.drawText(QRectF(19, 53, 42, 31), Qt::AlignCenter, QStringLiteral("FRONT"));
    painter.drawText(QRectF(64, 53, 42, 31), Qt::AlignCenter, QStringLiteral("RIGHT"));

    const auto drawButton = [&](const QRectF& box, Region region, const QString& label) {
        painter.setPen(QPen(hovered_ == region ? QColor("#D7A62B") : QColor("#4A5146"), 1));
        painter.setBrush(hovered_ == region ? QColor("#39331D") : QColor("#171D17"));
        painter.drawRoundedRect(box, 3, 3);
        painter.setPen(QColor("#D8D4C8")); painter.drawText(box, Qt::AlignCenter, label);
    };
    drawButton({7, 110, 34, 23}, Region::isometric, QStringLiteral("ISO"));
    drawButton({46, 110, 34, 23}, Region::fit, QStringLiteral("FIT"));
    drawButton({85, 110, 34, 23}, Region::projection, perspective_ ? QStringLiteral("P") : QStringLiteral("O"));
}

void ViewCube::mousePressEvent(QMouseEvent* event)
{
    const Region region = regionAt(event->position());
    if (cameraHandler_) {
        if (region == Region::top) cameraHandler_(viewport::CameraView::top);
        else if (region == Region::front) cameraHandler_(viewport::CameraView::front);
        else if (region == Region::right) cameraHandler_(viewport::CameraView::right);
        else if (region == Region::isometric) cameraHandler_(viewport::CameraView::isometric);
    }
    if (region == Region::fit && fitHandler_) fitHandler_();
    if (region == Region::projection && projectionHandler_) {
        perspective_ = !perspective_;
        projectionHandler_(perspective_);
        update();
    }
}

void ViewCube::mouseMoveEvent(QMouseEvent* event)
{
    hovered_ = regionAt(event->position());
    setCursor(hovered_ == Region::none ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void ViewCube::leaveEvent(QEvent*)
{
    hovered_ = Region::none;
    unsetCursor();
    update();
}

ViewCube::Region ViewCube::regionAt(const QPointF& point) const
{
    if (QRectF(7, 110, 34, 23).contains(point)) return Region::isometric;
    if (QRectF(46, 110, 34, 23).contains(point)) return Region::fit;
    if (QRectF(85, 110, 34, 23).contains(point)) return Region::projection;
    if (point.y() < 58 && point.y() > 8) return Region::top;
    if (point.y() <= 101 && point.y() >= 40)
        return point.x() < 62 ? Region::front : Region::right;
    return Region::none;
}

} // namespace voelund::preview
