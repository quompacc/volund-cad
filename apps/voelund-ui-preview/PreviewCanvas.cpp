#include "PreviewCanvas.hpp"

#include <QLinearGradient>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

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
    painter.setBrush(QColor("#273029"));
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
    pendingPoint_.reset();
    transientRelation_.clear();
    if (mode == Mode::Sketch && entities_.isEmpty()) {
        entities_.append({SketchEntity::Kind::Rectangle, {-62, -31}, {48, 29}, true, true});
        entities_.append({SketchEntity::Kind::Circle, {-8, 0}, {12, 0}, false, false});
    }
    update();
}

void PreviewCanvas::setSketchTool(SketchTool tool)
{
    sketchTool_ = tool;
    pendingPoint_.reset();
    transientRelation_.clear();
    const QString name = [tool] {
        switch (tool) {
        case SketchTool::Select: return QStringLiteral("Select geometry");
        case SketchTool::Line: return QStringLiteral("Line: choose first point");
        case SketchTool::Rectangle: return QStringLiteral("Rectangle: choose first corner");
        case SketchTool::Circle: return QStringLiteral("Circle: choose center");
        case SketchTool::Arc: return QStringLiteral("Arc: choose center");
        case SketchTool::Constraint: return QStringLiteral("Relation: select geometry");
        case SketchTool::Dimension: return QStringLiteral("Dimension: select geometry");
        case SketchTool::Trim: return QStringLiteral("Trim: select segment to remove");
        case SketchTool::Offset: return QStringLiteral("Offset: select geometry");
        }
        return QString{};
    }();
    emit sketchStatusChanged(name);
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
    } else if (mode_ == Mode::Sketch && event->button() == Qt::LeftButton) {
        handleSketchClick(event->position());
    }
    QWidget::mousePressEvent(event);
}

void PreviewCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (mode_ == Mode::Sketch) {
        QString relation;
        hoverPoint_ = snapPoint(toSketch(event->position()), &relation);
        transientRelation_ = relation;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PreviewCanvas::paintBackground(QPainter& painter) const
{
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0, QColor("#29312B"));
    background.setColorAt(1, QColor("#202721"));
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

    painter.setPen(QPen(QColor("#BDBCAF"), 1.2));
    painter.setBrush(QColor("#D4D2C8")); painter.drawPolygon(top);
    painter.setBrush(sideSelected_ ? QColor("#B88C24") : QColor("#858C83")); painter.drawPolygon(left);
    painter.setBrush(QColor("#A9ADA4")); painter.drawPolygon(right);

    const auto towerTop = polygon({{-80, -90}, {15, -144}, {120, -86}, {23, -31}});
    const auto towerLeft = polygon({{-80, -90}, {23, -31}, {23, 85}, {-80, 26}});
    const auto towerRight = polygon({{23, -31}, {120, -86}, {120, 30}, {23, 85}});
    painter.setBrush(QColor("#E6E3D9")); painter.drawPolygon(towerTop);
    painter.setBrush(QColor("#A6AAA1")); painter.drawPolygon(towerLeft);
    painter.setBrush(QColor("#C5C5BA")); painter.drawPolygon(towerRight);

    if (sideSelected_) {
        painter.setPen(QPen(QColor("#F0C14B"), 2.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(left);
    } else {
        painter.setPen(QPen(QColor("#E2B43D"), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(top);
    }

    painter.restore();

    painter.setPen(QColor("#E7E2D6"));
    QFont caption = painter.font(); caption.setPointSizeF(10.5); caption.setWeight(QFont::DemiBold); painter.setFont(caption);
    painter.drawText(QRect(24, 22, 360, 25), Qt::AlignLeft, sideSelected_ ? QStringLiteral("SIDE FACE SELECTED") : QStringLiteral("BODY / PAD"));
    painter.setPen(QColor("#939A8F"));
    QFont small = painter.font(); small.setPointSizeF(9); small.setWeight(QFont::Normal); painter.setFont(small);
    painter.drawText(QRect(24, 44, 400, 24), Qt::AlignLeft, QStringLiteral("Click canvas to change selection"));
}

void PreviewCanvas::paintSketch(QPainter& painter) const
{
    const QRectF plane = sketchPlane();

    painter.setPen(QPen(QColor("#C9A33F"), 1.2));
    painter.setBrush(QColor("#E8E5DC"));
    painter.drawRoundedRect(plane, 2, 2);

    painter.save();
    painter.setClipRect(plane);
    painter.setPen(QPen(QColor(45, 65, 72, 18), 1));
    for (qreal x = plane.left(); x <= plane.right(); x += 25) painter.drawLine(QPointF(x, plane.top()), QPointF(x, plane.bottom()));
    for (qreal y = plane.top(); y <= plane.bottom(); y += 25) painter.drawLine(QPointF(plane.left(), y), QPointF(plane.right(), y));
    painter.setPen(QPen(QColor("#D65757"), 1.3)); painter.drawLine(QPointF(plane.left(), plane.center().y()), QPointF(plane.right(), plane.center().y()));
    painter.setPen(QPen(QColor("#4AAE69"), 1.3)); painter.drawLine(QPointF(plane.center().x(), plane.top()), QPointF(plane.center().x(), plane.bottom()));
    painter.restore();

    for (int index = 0; index < entities_.size(); ++index) {
        const auto& entity = entities_.at(index);
        const QPointF first = toScreen(entity.first);
        const QPointF second = toScreen(entity.second);
        const QColor geometryColor = entity.constrained ? QColor("#427A58") : QColor("#36443F");
        painter.setPen(QPen(index == selectedEntity_ ? QColor("#D59C1C") : geometryColor,
                            index == selectedEntity_ ? 2.8 : 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (entity.kind == SketchEntity::Kind::Line) {
            painter.drawLine(first, second);
            drawEndpoint(painter, first, entity.constrained); drawEndpoint(painter, second, entity.constrained);
        } else if (entity.kind == SketchEntity::Kind::Rectangle) {
            const QRectF box(first, second);
            painter.drawRect(box.normalized());
            const QRectF normalized = box.normalized();
            for (const QPointF& point : {normalized.topLeft(), normalized.topRight(), normalized.bottomLeft(), normalized.bottomRight()}) {
                drawEndpoint(painter, point, entity.constrained);
            }
        } else {
            const qreal radius = QLineF(first, second).length();
            const QRectF circle(first.x() - radius, first.y() - radius, radius * 2, radius * 2);
            if (entity.kind == SketchEntity::Kind::Circle) painter.drawEllipse(circle);
            else painter.drawArc(circle, 15 * 16, 285 * 16);
            drawEndpoint(painter, first, entity.constrained);
        }

        if (entity.dimensioned) {
            const QPointF middle = (first + second) / 2.0;
            const qreal value = QLineF(entity.first, entity.second).length();
            painter.setPen(Qt::NoPen); painter.setBrush(QColor("#E8E5DC"));
            painter.drawRoundedRect(QRectF(middle.x() - 35, middle.y() + 12, 70, 21), 4, 4);
            painter.setPen(QColor("#3E493D"));
            painter.drawText(QRectF(middle.x() - 35, middle.y() + 12, 70, 21), Qt::AlignCenter,
                             QStringLiteral("%1 mm").arg(value, 0, 'f', 1));
        }
    }

    if (pendingPoint_) {
        const QPointF first = toScreen(*pendingPoint_);
        const QPointF second = toScreen(hoverPoint_);
        painter.setPen(QPen(QColor("#C99620"), 1.7, Qt::DashLine));
        painter.setBrush(QColor(43, 167, 207, 18));
        if (sketchTool_ == SketchTool::Rectangle) painter.drawRect(QRectF(first, second).normalized());
        else if (sketchTool_ == SketchTool::Circle || sketchTool_ == SketchTool::Arc) {
            const qreal radius = QLineF(first, second).length();
            const QRectF circle(first.x() - radius, first.y() - radius, radius * 2, radius * 2);
            if (sketchTool_ == SketchTool::Circle) painter.drawEllipse(circle); else painter.drawArc(circle, 15 * 16, 285 * 16);
        } else painter.drawLine(first, second);
        drawEndpoint(painter, second, !transientRelation_.isEmpty());
    }

    if (!transientRelation_.isEmpty()) {
        const QPointF labelPoint = toScreen(hoverPoint_) + QPointF(9, -24);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor("#1C6549"));
        painter.drawRoundedRect(QRectF(labelPoint, QSizeF(86, 21)), 4, 4);
        painter.setPen(QColor("#D9FFED"));
        painter.drawText(QRectF(labelPoint, QSizeF(86, 21)), Qt::AlignCenter, transientRelation_);
    }

    painter.setPen(QColor("#E7E2D6"));
    QFont caption = painter.font(); caption.setPointSizeF(10.5); caption.setWeight(QFont::DemiBold); painter.setFont(caption);
    painter.drawText(QRect(24, 22, 340, 25), Qt::AlignLeft, QStringLiteral("SKETCH 01  ·  TOP FACE"));
    painter.setPen(QColor("#F0B44D"));
    QFont small = painter.font(); small.setPointSizeF(9); small.setWeight(QFont::Normal); painter.setFont(small);
    painter.drawText(QRect(24, 44, 400, 24), Qt::AlignLeft, QStringLiteral("Under-constrained  ·  4 DOF"));
}

QRectF PreviewCanvas::sketchPlane() const
{
    const qreal canvasWidth = std::min(width() * 0.72, 720.0);
    const qreal canvasHeight = std::min(height() * 0.72, 500.0);
    return {(width() - canvasWidth) / 2.0, (height() - canvasHeight) / 2.0, canvasWidth, canvasHeight};
}

QPointF PreviewCanvas::toSketch(const QPointF& screenPoint) const
{
    constexpr qreal pixelsPerMillimetre = 2.5;
    const QPointF relative = screenPoint - sketchPlane().center();
    return {relative.x() / pixelsPerMillimetre, -relative.y() / pixelsPerMillimetre};
}

QPointF PreviewCanvas::toScreen(const QPointF& sketchPoint) const
{
    constexpr qreal pixelsPerMillimetre = 2.5;
    return sketchPlane().center() + QPointF(sketchPoint.x() * pixelsPerMillimetre, -sketchPoint.y() * pixelsPerMillimetre);
}

QPointF PreviewCanvas::snapPoint(const QPointF& sketchPoint, QString* relation) const
{
    constexpr qreal grid = 5.0;
    QPointF snapped(std::round(sketchPoint.x() / grid) * grid, std::round(sketchPoint.y() / grid) * grid);
    if (relation) *relation = QStringLiteral("Grid");

    for (const auto& entity : entities_) {
        for (const QPointF& endpoint : {entity.first, entity.second}) {
            if (QLineF(sketchPoint, endpoint).length() < 4.0) {
                if (relation) *relation = QStringLiteral("Coincident");
                return endpoint;
            }
        }
    }
    if (pendingPoint_) {
        if (std::abs(sketchPoint.x() - pendingPoint_->x()) < 2.5) {
            snapped.setX(pendingPoint_->x());
            if (relation) *relation = QStringLiteral("Vertical");
        } else if (std::abs(sketchPoint.y() - pendingPoint_->y()) < 2.5) {
            snapped.setY(pendingPoint_->y());
            if (relation) *relation = QStringLiteral("Horizontal");
        }
    }
    return snapped;
}

int PreviewCanvas::nearestEntity(const QPointF& sketchPoint) const
{
    int nearest = -1;
    qreal bestDistance = 18.0;
    for (int index = 0; index < entities_.size(); ++index) {
        const auto& entity = entities_.at(index);
        const QPointF middle = (entity.first + entity.second) / 2.0;
        const qreal distance = std::min({QLineF(sketchPoint, entity.first).length(),
                                        QLineF(sketchPoint, entity.second).length(),
                                        QLineF(sketchPoint, middle).length()});
        if (distance < bestDistance) {
            nearest = index;
            bestDistance = distance;
        }
    }
    return nearest;
}

void PreviewCanvas::handleSketchClick(const QPointF& screenPoint)
{
    QString relation;
    const QPointF point = snapPoint(toSketch(screenPoint), &relation);
    if (sketchTool_ == SketchTool::Select) {
        selectedEntity_ = nearestEntity(point);
        emit sketchStatusChanged(selectedEntity_ >= 0 ? QStringLiteral("Geometry selected") : QStringLiteral("Selection cleared"));
    } else if (sketchTool_ == SketchTool::Trim) {
        const int index = nearestEntity(point);
        if (index >= 0) entities_.removeAt(index);
        selectedEntity_ = -1;
        emit sketchStatusChanged(index >= 0 ? QStringLiteral("Segment trimmed") : QStringLiteral("No segment near cursor"));
    } else if (sketchTool_ == SketchTool::Dimension || sketchTool_ == SketchTool::Constraint) {
        const int index = nearestEntity(point);
        selectedEntity_ = index;
        if (index >= 0) {
            if (sketchTool_ == SketchTool::Dimension) entities_[index].dimensioned = true;
            else entities_[index].constrained = true;
        }
        emit sketchStatusChanged(index >= 0
            ? (sketchTool_ == SketchTool::Dimension ? QStringLiteral("Dimension added") : QStringLiteral("Relation applied"))
            : QStringLiteral("Select geometry"));
    } else if (sketchTool_ == SketchTool::Offset) {
        const int index = nearestEntity(point);
        if (index >= 0) {
            auto copy = entities_.at(index);
            copy.first += QPointF(8, 8); copy.second += QPointF(8, 8); copy.dimensioned = false;
            entities_.append(copy); selectedEntity_ = static_cast<int>(entities_.size() - 1);
        }
        emit sketchStatusChanged(index >= 0 ? QStringLiteral("Offset preview created") : QStringLiteral("Select geometry to offset"));
    } else if (!pendingPoint_) {
        pendingPoint_ = point;
        hoverPoint_ = point;
        emit sketchStatusChanged(QStringLiteral("Choose second point  ·  snapping enabled"));
    } else {
        SketchEntity::Kind kind = SketchEntity::Kind::Line;
        if (sketchTool_ == SketchTool::Rectangle) kind = SketchEntity::Kind::Rectangle;
        else if (sketchTool_ == SketchTool::Circle) kind = SketchEntity::Kind::Circle;
        else if (sketchTool_ == SketchTool::Arc) kind = SketchEntity::Kind::Arc;
        const bool constrained = relation == QStringLiteral("Coincident") || relation == QStringLiteral("Horizontal")
            || relation == QStringLiteral("Vertical");
        entities_.append({kind, *pendingPoint_, point, false, constrained});
        selectedEntity_ = static_cast<int>(entities_.size() - 1);
        pendingPoint_.reset();
        transientRelation_.clear();
        emit sketchStatusChanged(constrained ? QStringLiteral("Geometry added  ·  relation inferred")
                                             : QStringLiteral("Geometry added"));
    }
    update();
}

void PreviewCanvas::paintViewCube(QPainter& painter) const
{
    const QPointF center(width() - 70.0, 60.0);
    const auto top = polygon({{center.x() - 27, center.y() - 12}, {center.x(), center.y() - 28}, {center.x() + 27, center.y() - 12}, {center.x(), center.y() + 4}});
    const auto left = polygon({{center.x() - 27, center.y() - 12}, {center.x(), center.y() + 4}, {center.x(), center.y() + 34}, {center.x() - 27, center.y() + 18}});
    const auto right = polygon({{center.x(), center.y() + 4}, {center.x() + 27, center.y() - 12}, {center.x() + 27, center.y() + 18}, {center.x(), center.y() + 34}});
    painter.setPen(QPen(QColor("#777E73"), 1));
    painter.setBrush(QColor("#D8D5C9")); painter.drawPolygon(top);
    painter.setBrush(QColor("#747D73")); painter.drawPolygon(left);
    painter.setBrush(QColor("#9CA297")); painter.drawPolygon(right);
    painter.setPen(QColor("#293029"));
    QFont font = painter.font(); font.setPointSizeF(7.5); font.setWeight(QFont::DemiBold); painter.setFont(font);
    painter.drawText(QRectF(center.x() - 24, center.y() - 23, 48, 20), Qt::AlignCenter, QStringLiteral("TOP"));
}

void PreviewCanvas::paintNavigation(QPainter& painter) const
{
    const QRectF bar(width() / 2.0 - 114, height() - 44, 228, 30);
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.setBrush(QColor(18, 23, 28, 205));
    painter.drawRoundedRect(bar, 7, 7);
    painter.setPen(QColor("#AAAFA4"));
    QFont font = painter.font(); font.setPointSizeF(9); painter.setFont(font);
    painter.drawText(bar, Qt::AlignCenter, QStringLiteral("◉  Orbit     ▣  Pan     ⊕  Zoom     ⌂  Fit"));
}

} // namespace voelund::preview
