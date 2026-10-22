// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ViewCube.hpp"
#include "OverlayCard.hpp"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTimer>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace voelund::desktop {
namespace {

struct Vec final {
    double x, y, z;
};
Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec operator*(double s, Vec v) { return {s * v.x, s * v.y, s * v.z}; }
double dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec cross(Vec a, Vec b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec normalized(Vec v) {
    const double length = std::sqrt(dot(v, v));
    return length > 0.0 ? (1.0 / length) * v : v;
}
Vec vec(const ViewCube::Direction& d) {
    return {static_cast<double>(d[0]), static_cast<double>(d[1]), static_cast<double>(d[2])};
}

// Model axes: Z up; VORNE looks from -Y like OCCT's front view. `a` and `b` are the face's
// right and up directions as seen when looking straight at it, so labels read upright.
struct Face final {
    ViewCube::Direction n, a, b;
    const char* label;
};
const std::array<Face, 6> faces{{{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, "OBEN"},
                                 {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, "UNTEN"},
                                 {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, "VORNE"},
                                 {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, "HINTEN"},
                                 {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, "RECHTS"},
                                 {{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}, "LINKS"}}};
constexpr double bands[4] = {-1.0, -0.6, 0.6, 1.0}; // Corner, middle and corner cells.

constexpr QPointF cubeCenter{72, 82};
constexpr double cubeScale = 34.0;
const QRectF rollLeftBox{4, 4, 26, 26};
const QRectF rollRightBox{114, 4, 26, 26};
const QRectF homeBox{12, 148, 40, 26};
const QRectF fitBox{58, 148, 74, 26};

struct Basis final {
    Vec x, y, z;
};
Basis basis(const viewport::CameraFrame& frame) {
    const Vec z = normalized({frame.toEye[0], frame.toEye[1], frame.toEye[2]});
    const Vec up{frame.up[0], frame.up[1], frame.up[2]};
    const Vec y = normalized(up + (-dot(up, z)) * z);
    return {cross(y, z), y, z};
}
QPointF project(const Basis& b, Vec v) {
    return {cubeCenter.x() + cubeScale * dot(v, b.x), cubeCenter.y() - cubeScale * dot(v, b.y)};
}
Vec facePoint(const Face& face, double s, double t) {
    return vec(face.n) + s * vec(face.a) + t * vec(face.b);
}
QPolygonF faceQuad(const Basis& b, const Face& face, double low, double high) {
    return QPolygonF{
        {project(b, facePoint(face, low, high)), project(b, facePoint(face, high, high)),
         project(b, facePoint(face, high, low)), project(b, facePoint(face, low, low))}};
}

struct Cell final {
    QPolygonF polygon;
    ViewCube::Direction direction;
    const Face* face;
};
// Visible cells, back to front.
std::vector<Cell> visibleCells(const Basis& b) {
    std::vector<const Face*> visible;
    for (const auto& face : faces)
        if (dot(vec(face.n), b.z) > 0.02)
            visible.push_back(&face);
    std::ranges::sort(visible, [&](const Face* left, const Face* right) {
        return dot(vec(left->n), b.z) < dot(vec(right->n), b.z);
    });
    std::vector<Cell> cells;
    for (const Face* face : visible)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                ViewCube::Direction direction{};
                for (int axis = 0; axis < 3; ++axis)
                    direction[static_cast<std::size_t>(axis)] =
                        face->n[static_cast<std::size_t>(axis)] +
                        (i - 1) * face->a[static_cast<std::size_t>(axis)] +
                        (j - 1) * face->b[static_cast<std::size_t>(axis)];
                const QPolygonF polygon{{project(b, facePoint(*face, bands[i], bands[j + 1])),
                                         project(b, facePoint(*face, bands[i + 1], bands[j + 1])),
                                         project(b, facePoint(*face, bands[i + 1], bands[j])),
                                         project(b, facePoint(*face, bands[i], bands[j]))}};
                cells.push_back({polygon, direction, face});
            }
    return cells;
}

QPointF centroid(const QPolygonF& polygon) {
    QPointF sum;
    for (const auto& point : polygon)
        sum += point;
    return sum / static_cast<double>(polygon.size());
}

} // namespace

ViewCube::ViewCube(viewport::CadViewport& viewport)
    : QWidget(&viewport), viewport_(viewport), tokens_(themeTokens(ThemeMode::light)) {
    setObjectName(QStringLiteral("viewCube"));
    setFixedSize(144, overlay::cubeHeight);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    setToolTip(tr("Ansicht: Fläche, Kante oder Ecke anklicken, ziehen zum Drehen"));
    viewport.installEventFilter(this);
    placeInCorner();
    frame_ = viewport_.cameraFrame();
    // The cube follows every camera change (orbit, zoom, standard views).
    follow_ = new QTimer(this);
    connect(follow_, &QTimer::timeout, this, [this] { followCamera(); });
    follow_->start(40);
}

void ViewCube::setTokens(const ThemeTokens& tokens) {
    tokens_ = tokens;
    update();
}

void ViewCube::followCamera() {
    const auto frame = viewport_.cameraFrame();
    const auto same = [](const double* a, const double* b) {
        return std::abs(a[0] - b[0]) < 1e-9 && std::abs(a[1] - b[1]) < 1e-9 &&
               std::abs(a[2] - b[2]) < 1e-9;
    };
    if (same(frame.toEye, frame_.toEye) && same(frame.up, frame_.up))
        return;
    frame_ = frame;
    update();
}

bool ViewCube::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        placeInCorner();
    return QWidget::eventFilter(watched, event);
}

void ViewCube::placeInCorner() {
    move(viewport_.width() - width() - 12, overlay::cubeTop); // U14: below the header card
    raise();
}

std::optional<QPointF> ViewCube::cellCenter(const Direction& direction) const {
    for (const auto& cell : visibleCells(basis(viewport_.cameraFrame())))
        if (cell.direction == direction)
            return centroid(cell.polygon);
    return std::nullopt;
}
QPointF ViewCube::rollLeftCenter() const { return rollLeftBox.center(); }
QPointF ViewCube::rollRightCenter() const { return rollRightBox.center(); }
QPointF ViewCube::homeCenter() const { return homeBox.center(); }
QPointF ViewCube::fitCenter() const { return fitBox.center(); }

void ViewCube::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const Basis b = basis(frame_);
    const auto cells = visibleCells(b);
    const bool hoverCell = hovered_.direction.has_value();

    for (const auto& cell : cells) {
        const bool lit = hoverCell && cell.direction == *hovered_.direction;
        // Faces turned away from the eye are a little darker.
        const double facing = dot(vec(cell.face->n), b.z);
        const QColor base = tokens_.cubeTop.darker(100 + static_cast<int>((1.0 - facing) * 22));
        painter.setPen(QPen(tokens_.line, 0.6));
        painter.setBrush(lit ? tokens_.accentSoft : base);
        painter.drawPolygon(cell.polygon);
    }
    QFont font(uiFontFamily());
    font.setWeight(QFont::DemiBold);
    font.setPixelSize(24);
    for (const auto& face : faces) {
        const double facing = dot(vec(face.n), b.z);
        if (facing <= 0.02)
            continue;
        painter.setPen(QPen(tokens_.cubeEdge, 1.3));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(faceQuad(b, face, -1.0, 1.0));
        if (facing < 0.3)
            continue; // Too steep to read.
        QTransform transform;
        const QPolygonF label{{QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100)}};
        if (!QTransform::quadToQuad(label, faceQuad(b, face, -0.9, 0.9), transform))
            continue;
        painter.save();
        painter.setTransform(transform, true);
        painter.setFont(font);
        const bool lit = hoverCell && *hovered_.direction == face.n;
        painter.setPen(lit ? tokens_.accentInk : tokens_.muted);
        painter.drawText(QRectF(0, 0, 100, 100), Qt::AlignCenter, tr(face.label));
        painter.restore();
    }

    const auto buttonFrame = [&](const QRectF& box, Button button) {
        const bool lit = hovered_.button == button;
        painter.setPen(QPen(lit ? tokens_.accent : tokens_.line, 1));
        painter.setBrush(lit ? tokens_.accentSoft : tokens_.panel);
        painter.drawRoundedRect(box, box.height() / 2, box.height() / 2); // U14: pills
        painter.setPen(QPen(lit ? tokens_.accentInk : tokens_.ink, 1.5, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
    };
    const auto rollArrow = [&](const QRectF& box, bool left) {
        const QRectF arc = box.adjusted(6, 6, -6, -6);
        painter.drawArc(arc, left ? 30 * 16 : 150 * 16, left ? 240 * 16 : -240 * 16);
        const double angle = (left ? 30.0 : 150.0) * std::numbers::pi / 180.0;
        const QPointF tip(arc.center().x() + arc.width() / 2 * std::cos(angle),
                          arc.center().y() - arc.height() / 2 * std::sin(angle));
        const double side = left ? 1.0 : -1.0;
        painter.drawLine(tip, tip + QPointF(-3.5 * side, -1.0));
        painter.drawLine(tip, tip + QPointF(-0.5 * side, -4.0));
    };
    // Left turns the picture counter-clockwise (↺), right clockwise (↻).
    buttonFrame(rollLeftBox, Button::rollLeft);
    rollArrow(rollLeftBox, false);
    buttonFrame(rollRightBox, Button::rollRight);
    rollArrow(rollRightBox, true);
    buttonFrame(homeBox, Button::home);
    QPainterPath house;
    const QPointF c = homeBox.center();
    house.moveTo(c + QPointF(-6, 0));
    house.lineTo(c + QPointF(0, -6));
    house.lineTo(c + QPointF(6, 0));
    house.moveTo(c + QPointF(-4, -1.5));
    house.lineTo(c + QPointF(-4, 6));
    house.lineTo(c + QPointF(4, 6));
    house.lineTo(c + QPointF(4, -1.5));
    painter.drawPath(house);
    buttonFrame(fitBox, Button::fit);
    QFont buttonFont(uiFontFamily());
    buttonFont.setPixelSize(11);
    buttonFont.setWeight(QFont::DemiBold);
    painter.setFont(buttonFont);
    painter.drawText(fitBox, Qt::AlignCenter, tr("Alles"));
}

ViewCube::Hit ViewCube::hitAt(const QPointF& point) const {
    if (rollLeftBox.contains(point))
        return {Button::rollLeft, {}};
    if (rollRightBox.contains(point))
        return {Button::rollRight, {}};
    if (homeBox.contains(point))
        return {Button::home, {}};
    if (fitBox.contains(point))
        return {Button::fit, {}};
    // The current camera, not the last painted one: a quick second click must hit what is
    // there now.
    const auto cells = visibleCells(basis(viewport_.cameraFrame()));
    for (auto cell = cells.rbegin(); cell != cells.rend(); ++cell) // Front first.
        if (cell->polygon.containsPoint(point, Qt::OddEvenFill))
            return {Button::none, cell->direction};
    return {};
}

void ViewCube::lookAlong(const Direction& direction) {
    const Vec toEye = normalized(vec(direction));
    // Z is up except when looking straight down or up; then +Y is up on screen.
    const Vec up = std::abs(toEye.z) > 0.99 ? Vec{0, 1, 0} : Vec{0, 0, 1};
    viewport_.setCameraFrame({{toEye.x, toEye.y, toEye.z}, {up.x, up.y, up.z}});
}

void ViewCube::mousePressEvent(QMouseEvent* event) {
    const Hit hit = hitAt(event->position());
    switch (hit.button) {
    case Button::rollLeft:
        viewport_.rollView(-std::numbers::pi / 2);
        break;
    case Button::rollRight:
        viewport_.rollView(std::numbers::pi / 2);
        break;
    case Button::home:
        viewport_.setCameraView(viewport::CameraView::isometric);
        break;
    case Button::fit:
        viewport_.fitView();
        break;
    case Button::none:
        if (!hit.direction) {
            event->ignore();
            return;
        }
        pressed_ = event->position();
        lastDrag_ = event->position();
        dragging_ = false;
        break;
    }
    event->accept();
}

void ViewCube::mouseMoveEvent(QMouseEvent* event) {
    if (pressed_ && (event->buttons() & Qt::LeftButton)) {
        const QPointF delta = event->position() - lastDrag_;
        if (!dragging_ && (event->position() - *pressed_).manhattanLength() > 3)
            dragging_ = true;
        if (dragging_) {
            // Grabbing the cube: dragging right turns its front to the right.
            constexpr double radiansPerPixel = 0.012;
            viewport_.orbitBy(delta.x() * radiansPerPixel, delta.y() * radiansPerPixel);
            lastDrag_ = event->position();
        }
        return;
    }
    const Hit hit = hitAt(event->position());
    if (hit.button != hovered_.button || hit.direction != hovered_.direction) {
        hovered_ = hit;
        setCursor(hit.button == Button::none && !hit.direction ? Qt::ArrowCursor
                                                               : Qt::PointingHandCursor);
        update();
    }
}

void ViewCube::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_ && !dragging_) {
        const Hit hit = hitAt(*pressed_);
        if (hit.direction)
            lookAlong(*hit.direction);
    }
    pressed_.reset();
    dragging_ = false;
    event->accept();
}

void ViewCube::leaveEvent(QEvent*) {
    hovered_ = {};
    unsetCursor();
    update();
}

} // namespace voelund::desktop
