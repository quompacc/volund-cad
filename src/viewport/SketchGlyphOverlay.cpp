// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchGlyphOverlay.hpp"

#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <map>

namespace voelund::viewport {
namespace {

// SolidCraft's layout: 0.9 of a box beside the curve, steps of box + 2 along it, a leader
// beyond 1.6 boxes, a hit radius of 0.6 boxes. The box is 17 px (SolidCraft 15) so its letter
// reads well (user report 2026-10-25: labels too small).
constexpr double box = 17.0;
constexpr int steps = 12;

QPointF unit(QPointF vector) {
    const double length = std::hypot(vector.x(), vector.y());
    return length > 1e-9 ? vector / length : QPointF(1, 0);
}

struct Taken final {
    std::vector<QPointF> boxes;
    std::vector<QRectF> labels; // as SolidCraft: a symbol never covers a dimension's value
};

bool free(QPointF centre, const Taken& taken) {
    const QRectF area(centre - QPointF(box / 2 + 2, box / 2 + 2), QSizeF(box + 4, box + 4));
    return std::ranges::none_of(taken.boxes,
                                [&](QPointF other) {
                                    return std::hypot(centre.x() - other.x(),
                                                      centre.y() - other.y()) < box + 1;
                                }) &&
           std::ranges::none_of(taken.labels,
                                [&](const QRectF& label) { return label.intersects(area); });
}

// Beside the curve (either side), then stepping along it until the spot is free.
QPointF freeSpot(QPointF base, QPointF along, const Taken& taken) {
    const QPointF side(-along.y(), along.x());
    for (int step = 0; step <= steps; ++step)
        for (const double beside : {-0.9, 0.9})
            for (const int sign : {1, -1}) {
                const QPointF spot = base + side * box * beside + along * (box + 2) * step * sign;
                if (free(spot, taken))
                    return spot;
            }
    return base - side * box * 0.9;
}

void line(QPainter& painter, double x1, double y1, double x2, double y2) {
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

void dot(QPainter& painter, QPointF at, double radius) {
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(at, radius, radius);
    painter.setBrush(Qt::NoBrush);
}

void letter(QPainter& painter, const QString& text) {
    QFont font = painter.font();
    font.setPixelSize(11);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(-6, -6, 12, 12), Qt::AlignCenter, text);
}

using Drawing = void (*)(QPainter&);

// Small drawings of the relations inside a box centred at (0, 0), about 11 px across.
const std::map<std::string, Drawing>& drawings() {
    static const std::map<std::string, Drawing> table{
        {"C", [](QPainter& p) { dot(p, {0, 0}, 2.5); }},
        {"O",
         [](QPainter& p) {
             dot(p, {0, 0}, 2.2);
             p.drawArc(QRectF(-5, -1, 10, 10), 30 * 16, 120 * 16);
         }},
        {"||",
         [](QPainter& p) {
             line(p, -3.5, 4.5, -0.5, -4.5);
             line(p, 0.5, 4.5, 3.5, -4.5);
         }},
        {"90",
         [](QPainter& p) {
             line(p, 0, -4.5, 0, 4);
             line(p, -4.5, 4, 4.5, 4);
         }},
        {"PB",
         [](QPainter& p) {
             line(p, 0, -4.5, 0, 4.5);
             line(p, -4.5, 0, 4.5, 0);
         }},
        {"=",
         [](QPainter& p) {
             line(p, -4, -2, 4, -2);
             line(p, -4, 2, 4, 2);
         }},
        {"T",
         [](QPainter& p) {
             p.drawEllipse(QPointF(0, 1.5), 3.5, 3.5);
             line(p, -5, -2, 5, -2);
         }},
        {"CC",
         [](QPainter& p) {
             p.drawEllipse(QPointF(0, 0), 1.8, 1.8);
             p.drawEllipse(QPointF(0, 0), 4.5, 4.5);
         }},
        {"CL",
         [](QPainter& p) {
             line(p, -5, 3, 5, -3);
             dot(p, {-2.5, 1.5}, 1.4);
             dot(p, {2.5, -1.5}, 1.4);
         }},
        {"M",
         [](QPainter& p) {
             line(p, -5, 3, 5, -3);
             dot(p, {0, 0}, 1.8);
         }},
        {"S",
         [](QPainter& p) {
             line(p, 0, -5, 0, 5);
             dot(p, {-3.5, 0}, 1.5);
             dot(p, {3.5, 0}, 1.5);
         }},
        {"F",
         [](QPainter& p) {
             p.drawRect(QRectF(-3.5, -0.5, 7, 5));
             p.drawArc(QRectF(-2.5, -5, 5, 7), 0, 180 * 16);
         }},
        {"HP", [](QPainter& p) { letter(p, QStringLiteral("H")); }},
        {"VP", [](QPainter& p) { letter(p, QStringLiteral("V")); }}};
    return table;
}

void drawSymbol(QPainter& painter, const std::string& key) {
    const auto found = drawings().find(key);
    if (found != drawings().end())
        found->second(painter);
    else
        letter(painter, QString::fromStdString(key)); // H, V and anything new
}

} // namespace

QString glyphName(const std::string& key) {
    static const std::map<std::string, QString> names{{"H", QObject::tr("Horizontal")},
                                                      {"V", QObject::tr("Vertikal")},
                                                      {"HP", QObject::tr("Horizontal (Punkte)")},
                                                      {"VP", QObject::tr("Vertikal (Punkte)")},
                                                      {"C", QObject::tr("Koinzident")},
                                                      {"O", QObject::tr("Punkt auf Objekt")},
                                                      {"||", QObject::tr("Parallel")},
                                                      {"90", QObject::tr("Senkrecht")},
                                                      {"=", QObject::tr("Gleich")},
                                                      {"T", QObject::tr("Tangential")},
                                                      {"CC", QObject::tr("Konzentrisch")},
                                                      {"CL", QObject::tr("Kollinear")},
                                                      {"M", QObject::tr("Mittelpunkt")},
                                                      {"S", QObject::tr("Symmetrisch")},
                                                      {"F", QObject::tr("Fixiert")},
                                                      {"PB", QObject::tr("Mittelsenkrechte")}};
    const auto found = names.find(key);
    return found != names.end() ? found->second : QString::fromStdString(key);
}

SketchGlyphOverlay::SketchGlyphOverlay(QWidget& viewport)
    : QWidget(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("sketchGlyphOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(viewport.rect());
    viewport.installEventFilter(this);
    hide();
}

void SketchGlyphOverlay::setGlyphs(std::vector<Glyph> glyphs) {
    glyphs_ = std::move(glyphs);
    if (hovered_ && std::ranges::find(glyphs_, *hovered_, &Glyph::id) == glyphs_.end())
        hovered_.reset();
    place();
    setVisible(!glyphs_.empty());
    lower(); // over the view, under its cards (tree, panels, timeline)
    update();
}

void SketchGlyphOverlay::setColors(const QColor& text, const QColor& accent, const QColor& panel) {
    text_ = text;
    accent_ = accent;
    panel_ = panel;
    update();
}

void SketchGlyphOverlay::place() {
    boxes_.clear();
    setProperty("glyphBoxes", QVariantList());
    if (!projector_ || glyphs_.empty())
        return;
    Taken taken{{}, obstacles_ ? obstacles_() : std::vector<QRectF>{}};
    for (std::size_t index = 0; index < glyphs_.size(); ++index)
        for (const SceneGlyph& anchor : glyphs_[index].anchors) {
            const auto base = projector_(anchor.x, anchor.y, anchor.z);
            const auto ahead = projector_(anchor.x + anchor.alongX, anchor.y + anchor.alongY,
                                          anchor.z + anchor.alongZ);
            if (!base || !ahead)
                continue;
            const QPointF centre = freeSpot(*base, unit(*ahead - *base), taken);
            taken.boxes.push_back(centre);
            boxes_.push_back(
                {index, QRectF(centre - QPointF(box / 2, box / 2), QSizeF(box, box)), *base});
        }
    // Test hooks: the boxes as the user sees them, without reading pixels.
    QVariantList rects;
    for (const Box& placed : boxes_)
        rects.push_back(placed.rect);
    setProperty("glyphBoxes", rects);
}

std::optional<std::uint64_t> SketchGlyphOverlay::glyphAt(QPointF position) const {
    for (const Box& placed : boxes_) {
        const QPointF centre = placed.rect.center();
        if (std::hypot(position.x() - centre.x(), position.y() - centre.y()) <= box * 0.6)
            return glyphs_[placed.glyph].id;
    }
    return std::nullopt;
}

bool SketchGlyphOverlay::hover(QPointF position) {
    const auto now = glyphAt(position);
    if (now == hovered_)
        return false;
    hovered_ = now;
    setProperty("hoveredGlyph", now ? QVariant::fromValue(*now) : QVariant());
    if (now)
        QToolTip::showText(viewport_.mapToGlobal(position.toPoint() + QPoint(12, 12)),
                           glyphName(std::ranges::find(glyphs_, *now, &Glyph::id)->key),
                           &viewport_);
    else
        QToolTip::hideText();
    update();
    return true;
}

std::vector<std::uint64_t> SketchGlyphOverlay::hoveredElements() const {
    if (!hovered_)
        return {};
    const auto glyph = std::ranges::find(glyphs_, *hovered_, &Glyph::id);
    return glyph != glyphs_.end() ? glyph->elements : std::vector<std::uint64_t>{};
}

std::vector<QRectF> SketchGlyphOverlay::boxes() const {
    std::vector<QRectF> result;
    for (const Box& placed : boxes_)
        result.push_back(placed.rect);
    return result;
}

bool SketchGlyphOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        setGeometry(viewport_.rect());
    return QWidget::eventFilter(watched, event);
}

void SketchGlyphOverlay::paintEvent(QPaintEvent*) {
    place(); // the camera may have moved since the last frame
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    for (const Box& placed : boxes_)
        paintBox(painter, glyphs_[placed.glyph], placed);
}

void SketchGlyphOverlay::paintBox(QPainter& painter, const Glyph& glyph, const Box& placed) const {
    const bool lit = glyph.selected || hovered_ == glyph.id;
    QColor ink = glyph.conflicting ? QColor(0xD6, 0x36, 0x36) : lit ? accent_ : text_;
    if (glyph.redundant && !lit)
        ink.setAlphaF(0.45F);
    const QPointF centre = placed.rect.center();
    if (std::hypot(centre.x() - placed.anchor.x(), centre.y() - placed.anchor.y()) > box * 1.6) {
        QColor leader = ink;
        leader.setAlphaF(0.5F);
        painter.setPen(QPen(leader, 0.8));
        painter.drawLine(placed.anchor, centre);
    }
    QColor fill = lit ? accent_ : panel_;
    fill.setAlphaF(lit ? 0.2F : 0.92F);
    painter.setPen(QPen(ink, 1.0));
    painter.setBrush(lit ? QBrush(fill) : QBrush(panel_));
    painter.drawRoundedRect(placed.rect, 3, 3);
    painter.save();
    painter.translate(centre);
    painter.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    drawSymbol(painter, glyph.key);
    painter.restore();
}

} // namespace voelund::viewport
