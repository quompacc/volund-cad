#include "ViewCube.hpp"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

namespace voelund::desktop {
namespace {

constexpr int margin = 16;
const QPolygonF topFace{{{12, 26}, {48, 8}, {84, 26}, {48, 44}}};
const QPolygonF frontFace{{{12, 26}, {48, 44}, {48, 82}, {12, 64}}};
const QPolygonF rightFace{{{84, 26}, {48, 44}, {48, 82}, {84, 64}}};
const QRectF isoButton{4, 92, 42, 26};
const QRectF fitButton{50, 92, 42, 26};

} // namespace

ViewCube::ViewCube(viewport::CadViewport& viewport)
    : QWidget(&viewport), viewport_(viewport), tokens_(themeTokens(ThemeMode::light)) {
    setObjectName(QStringLiteral("viewCube"));
    setFixedSize(96, 122);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setToolTip(tr("Standardansichten"));
    viewport.installEventFilter(this);
    placeInCorner();
}

void ViewCube::setTokens(const ThemeTokens& tokens) {
    tokens_ = tokens;
    update();
}

bool ViewCube::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        placeInCorner();
    return QWidget::eventFilter(watched, event);
}

void ViewCube::placeInCorner() {
    move(viewport_.width() - width() - margin, margin);
    raise();
}

void ViewCube::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto face = [&](const QPolygonF& polygon, Region region, const QColor& fill,
                          const QString& label, const QRectF& labelBox) {
        painter.setPen(QPen(hovered_ == region ? tokens_.accent : tokens_.cubeEdge, 1.2));
        painter.setBrush(hovered_ == region ? tokens_.accentSoft : fill);
        painter.drawPolygon(polygon);
        painter.setPen(hovered_ == region ? tokens_.accentInk : tokens_.muted);
        painter.drawText(labelBox, Qt::AlignCenter, label);
    };
    QFont font(uiFontFamily());
    font.setPixelSize(9);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    face(topFace, Region::top, tokens_.cubeTop, tr("OBEN"), {12, 14, 72, 24});
    font.setPixelSize(8); // Side labels share a narrow face each.
    painter.setFont(font);
    face(frontFace, Region::front, tokens_.cubeLeft, tr("VORNE"), {12, 44, 36, 28});
    face(rightFace, Region::right, tokens_.cubeRight, tr("RECHTS"), {48, 44, 36, 28});
    const auto button = [&](const QRectF& box, Region region, const QString& label) {
        painter.setPen(QPen(hovered_ == region ? tokens_.accent : tokens_.line, 1));
        painter.setBrush(hovered_ == region ? tokens_.accentSoft : tokens_.panel);
        painter.drawRoundedRect(box, 7, 7);
        painter.setPen(hovered_ == region ? tokens_.accentInk : tokens_.ink);
        painter.drawText(box, Qt::AlignCenter, label);
    };
    font.setPixelSize(11);
    painter.setFont(font);
    button(isoButton, Region::isometric, tr("ISO"));
    button(fitButton, Region::fit, tr("Alles"));
}

void ViewCube::mousePressEvent(QMouseEvent* event) {
    switch (regionAt(event->position())) {
    case Region::top:
        viewport_.setCameraView(viewport::CameraView::top);
        break;
    case Region::front:
        viewport_.setCameraView(viewport::CameraView::front);
        break;
    case Region::right:
        viewport_.setCameraView(viewport::CameraView::right);
        break;
    case Region::isometric:
        viewport_.setCameraView(viewport::CameraView::isometric);
        break;
    case Region::fit:
        viewport_.fitView();
        break;
    case Region::none:
        event->ignore();
        return;
    }
    event->accept();
}

void ViewCube::mouseMoveEvent(QMouseEvent* event) {
    const Region region = regionAt(event->position());
    if (region != hovered_) {
        hovered_ = region;
        setCursor(region == Region::none ? Qt::ArrowCursor : Qt::PointingHandCursor);
        update();
    }
}

void ViewCube::leaveEvent(QEvent*) {
    hovered_ = Region::none;
    unsetCursor();
    update();
}

ViewCube::Region ViewCube::regionAt(const QPointF& point) const {
    if (isoButton.contains(point))
        return Region::isometric;
    if (fitButton.contains(point))
        return Region::fit;
    if (topFace.containsPoint(point, Qt::OddEvenFill))
        return Region::top;
    if (frontFace.containsPoint(point, Qt::OddEvenFill))
        return Region::front;
    if (rightFace.containsPoint(point, Qt::OddEvenFill))
        return Region::right;
    return Region::none;
}

} // namespace voelund::desktop
