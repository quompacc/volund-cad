// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchIcons.hpp"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cmath>
#include <functional>
#include <numbers>

// User request 2026-10-05: symbols for the sketch tools. Same 24-unit grid and stroke as
// ShellIcons; geometry in ink, points and markers in the accent color.
namespace voelund::desktop {
namespace {

struct Pens final {
    QPainter& painter;
    QColor ink;
    QColor accent;

    void line(double x1, double y1, double x2, double y2) const {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
    void dot(double x, double y, double radius = 1.9) const {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawEllipse(QPointF(x, y), radius, radius);
        painter.restore();
    }
    void dashed(const std::function<void()>& draw) const {
        painter.save();
        auto pen = painter.pen();
        pen.setDashPattern({2.0, 2.0});
        pen.setWidthF(1.4);
        painter.setPen(pen);
        draw();
        painter.restore();
    }
    void accented(const std::function<void()>& draw) const {
        painter.save();
        auto pen = painter.pen();
        pen.setColor(accent);
        painter.setPen(pen);
        draw();
        painter.restore();
    }
    void arrowHead(double x, double y, double angle) const {
        const double spread = 0.5;
        line(x, y, x - 3.2 * std::cos(angle - spread), y - 3.2 * std::sin(angle - spread));
        line(x, y, x - 3.2 * std::cos(angle + spread), y - 3.2 * std::sin(angle + spread));
    }
    // A dimension with ticks and arrows between two points, offset along its normal.
    void dimension(double x1, double y1, double x2, double y2) const {
        accented([&] {
            const double angle = std::atan2(y2 - y1, x2 - x1);
            line(x1, y1, x2, y2);
            arrowHead(x2, y2, angle);
            arrowHead(x1, y1, angle + std::numbers::pi);
        });
    }
    void lock(double x, double y, bool open) const {
        painter.drawRoundedRect(QRectF(x - 5, y, 10, 8), 1.5, 1.5);
        QPainterPath shackle;
        shackle.moveTo(x - 3, y);
        shackle.lineTo(x - 3, y - 3);
        shackle.arcTo(QRectF(x - 3, y - 6, 6, 6), 180, -180);
        if (!open)
            shackle.lineTo(x + 3, y);
        painter.drawPath(shackle);
        dot(x, y + 4, 1.3);
    }
};

using Drawing = std::function<void(const Pens&)>;

const QHash<QString, Drawing>& drawings() {
    static const QHash<QString, Drawing> table{
        // Drawing tools.
        {"tool.select",
         [](const Pens& p) {
             QPainterPath arrow;
             arrow.moveTo(6, 3);
             arrow.lineTo(6, 19);
             arrow.lineTo(10, 15);
             arrow.lineTo(13, 21);
             arrow.lineTo(15.5, 20);
             arrow.lineTo(12.5, 14);
             arrow.lineTo(18, 14);
             arrow.closeSubpath();
             p.painter.drawPath(arrow);
         }},
        {"tool.line",
         [](const Pens& p) {
             p.line(5, 19, 19, 5);
             p.dot(5, 19);
             p.dot(19, 5);
         }},
        {"tool.polyline",
         [](const Pens& p) {
             QPainterPath path;
             path.moveTo(3.5, 18);
             path.lineTo(9, 8);
             path.lineTo(15, 16);
             path.lineTo(20.5, 6);
             p.painter.drawPath(path);
             for (const auto& [x, y] :
                  {std::pair{3.5, 18.0}, {9.0, 8.0}, {15.0, 16.0}, {20.5, 6.0}})
                 p.dot(x, y, 1.6);
         }},
        {"tool.rectangle",
         [](const Pens& p) {
             p.painter.drawRect(QRectF(4, 6, 16, 12));
             p.dot(4, 6);
             p.dot(20, 18);
         }},
        {"tool.center_rectangle",
         [](const Pens& p) {
             p.painter.drawRect(QRectF(4, 6, 16, 12));
             p.dashed([&] { p.line(4, 6, 12, 12); });
             p.dot(12, 12);
             p.dot(20, 18);
         }},
        {"tool.circle",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 8, 8);
             p.dot(12, 12);
         }},
        {"tool.center_arc",
         [](const Pens& p) {
             p.painter.drawArc(QRectF(4, 6, 16, 16), 0, 180 * 16);
             p.dashed([&] { p.line(12, 14, 20, 14); });
             p.dot(12, 14);
             p.dot(4, 14, 1.6);
             p.dot(20, 14, 1.6);
         }},
        {"tool.three_point_arc",
         [](const Pens& p) {
             p.painter.drawArc(QRectF(4, 6, 16, 16), 0, 180 * 16);
             p.dot(4, 14, 1.6);
             p.dot(12, 6, 1.6);
             p.dot(20, 14, 1.6);
         }},
        {"tool.ellipse",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 9, 5.5);
             p.dot(12, 12);
         }},
        {"tool.slot",
         [](const Pens& p) {
             p.painter.drawRoundedRect(QRectF(2.5, 7.5, 19, 9), 4.5, 4.5);
             p.dot(7, 12, 1.5);
             p.dot(17, 12, 1.5);
         }},
        {"tool.bspline", // S21: a smooth curve over its dashed control polygon
         [](const Pens& p) {
             p.dashed([&] {
                 p.line(3, 19, 7, 5);
                 p.line(7, 5, 17, 5);
                 p.line(17, 5, 21, 19);
             });
             QPainterPath curve;
             curve.moveTo(3, 19);
             curve.cubicTo(7, 5, 17, 5, 21, 19);
             p.painter.drawPath(curve);
         }},
        {"tool.point", // S15/U14: a point with its cross, not the word "Punkt"
         [](const Pens& p) {
             p.line(12, 4, 12, 8);
             p.line(12, 16, 12, 20);
             p.line(4, 12, 8, 12);
             p.line(16, 12, 20, 12);
             p.dot(12, 12, 2.2);
         }},
        {"tool.polygon",
         [](const Pens& p) {
             QPolygonF hexagon;
             for (int corner = 0; corner < 6; ++corner) {
                 const double angle = corner * std::numbers::pi / 3;
                 hexagon << QPointF(12 + 8.5 * std::cos(angle), 12 + 8.5 * std::sin(angle));
             }
             p.painter.drawPolygon(hexagon);
             p.dot(12, 12, 1.5);
         }},
        // Relations.
        {"horizontal",
         [](const Pens& p) {
             p.line(4, 14, 20, 14);
             p.dot(4, 14);
             p.dot(20, 14);
             p.accented([&] {
                 p.line(9, 6, 9, 10);
                 p.line(15, 6, 15, 10);
                 p.line(9, 8, 15, 8);
             });
         }},
        {"vertical",
         [](const Pens& p) {
             p.line(10, 4, 10, 20);
             p.dot(10, 4);
             p.dot(10, 20);
             p.accented([&] {
                 p.line(15, 9, 17, 15);
                 p.line(17, 15, 19, 9);
             });
         }},
        {"free",
         [](const Pens& p) {
             p.line(5, 19, 19, 5);
             p.accented([&] {
                 p.line(14, 15, 20, 21);
                 p.line(20, 15, 14, 21);
             });
         }},
        {"parallel",
         [](const Pens& p) {
             p.line(4, 15, 15, 4);
             p.line(9, 20, 20, 9);
         }},
        {"collinear",
         [](const Pens& p) {
             p.line(3, 19, 10, 12);
             p.line(14, 8, 21, 3);
             p.dashed([&] { p.line(10, 12, 14, 8); });
         }},
        {"perpendicular",
         [](const Pens& p) {
             p.line(4, 19, 20, 19);
             p.line(12, 19, 12, 4);
             p.accented([&] {
                 p.line(12, 15, 16, 15);
                 p.line(16, 15, 16, 19);
             });
         }},
        {"perpendicular_bisector",
         [](const Pens& p) {
             p.line(3, 16, 21, 16);
             p.dashed([&] { p.line(12, 21, 12, 3); });
             p.dot(12, 16);
         }},
        {"coincident",
         [](const Pens& p) {
             p.line(4, 19, 12, 11);
             p.line(12, 11, 20, 17);
             p.dot(12, 11, 2.6);
         }},
        {"midpoint",
         [](const Pens& p) {
             p.line(3, 17, 21, 7);
             p.dot(12, 12, 2.4);
             p.accented([&] { p.line(12, 12, 9, 6.5); });
         }},
        {"point_on_object",
         [](const Pens& p) {
             p.painter.drawArc(QRectF(3, 6, 18, 18), 20 * 16, 140 * 16);
             p.dot(12, 6, 2.4);
         }},
        {"tangent",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 14), 6, 6);
             p.line(3, 8, 21, 8);
             p.dot(12, 8, 1.8);
         }},
        {"concentric",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 9, 9);
             p.painter.drawEllipse(QPointF(12, 12), 4.5, 4.5);
             p.dot(12, 12, 1.5);
         }},
        {"equal",
         [](const Pens& p) {
             p.line(5, 9, 19, 9);
             p.line(5, 15, 19, 15);
         }},
        {"symmetric",
         [](const Pens& p) {
             p.dashed([&] { p.line(12, 3, 12, 21); });
             p.dot(6, 12, 2.2);
             p.dot(18, 12, 2.2);
             p.accented([&] {
                 p.line(8, 12, 10, 12);
                 p.line(14, 12, 16, 12);
             });
         }},
        {"horizontal_points",
         [](const Pens& p) {
             p.dashed([&] { p.line(5, 12, 19, 12); });
             p.dot(5, 12, 2.3);
             p.dot(19, 12, 2.3);
         }},
        {"vertical_points",
         [](const Pens& p) {
             p.dashed([&] { p.line(12, 5, 12, 19); });
             p.dot(12, 5, 2.3);
             p.dot(12, 19, 2.3);
         }},
        {"fixed",
         [](const Pens& p) {
             p.line(4, 20, 20, 20);
             for (double x : {6.0, 10.0, 14.0, 18.0})
                 p.line(x, 20, x - 2.5, 23);
             p.line(12, 20, 12, 8);
             p.dot(12, 7, 2.6);
         }},
        {"lock_point", [](const Pens& p) { p.lock(12, 11, false); }},
        {"unlock_point", [](const Pens& p) { p.lock(12, 11, true); }},
        {"construction",
         [](const Pens& p) {
             p.dashed([&] {
                 p.line(4, 19, 20, 5);
                 p.painter.drawEllipse(QPointF(12, 12), 4, 4);
             });
         }},
        {"remove_constraint",
         [](const Pens& p) {
             p.line(4, 19, 18, 19);
             p.line(11, 19, 11, 6);
             p.accented([&] { p.line(4, 4, 20, 20); });
         }},
        // Dimensions.
        {"line_length",
         [](const Pens& p) {
             p.line(4, 17, 20, 17);
             p.line(4, 14, 4, 20);
             p.line(20, 14, 20, 20);
             p.dimension(5, 8, 19, 8);
         }},
        {"point_distance",
         [](const Pens& p) {
             p.dot(5, 18);
             p.dot(19, 6);
             p.dimension(7, 16, 17, 8);
         }},
        {"point_distance_x",
         [](const Pens& p) {
             p.dot(5, 18);
             p.dot(19, 10);
             p.dashed([&] {
                 p.line(5, 18, 5, 5);
                 p.line(19, 10, 19, 5);
             });
             p.dimension(6, 6, 18, 6);
         }},
        {"point_distance_y",
         [](const Pens& p) {
             p.dot(6, 19);
             p.dot(14, 5);
             p.dashed([&] {
                 p.line(6, 19, 19, 19);
                 p.line(14, 5, 19, 5);
             });
             p.dimension(19, 18, 19, 6);
         }},
        {"point_line_distance",
         [](const Pens& p) {
             p.line(3, 19, 21, 19);
             p.dot(12, 5);
             p.dimension(12, 7, 12, 18);
         }},
        {"circle_radius",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(11, 13), 8, 8);
             p.dot(11, 13, 1.5);
             p.dimension(11, 13, 16.6, 7.4);
         }},
        {"radius",
         [](const Pens& p) {
             p.painter.drawArc(QRectF(3, 5, 16, 16), 0, 180 * 16);
             p.dot(11, 13, 1.5);
             p.dimension(11, 13, 16.6, 7.4);
         }},
        {"diameter",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 8, 8);
             p.dimension(6.4, 17.6, 17.6, 6.4);
         }},
        {"angle",
         [](const Pens& p) {
             p.line(4, 19, 20, 19);
             p.line(4, 19, 16, 6);
             p.accented([&] { p.painter.drawArc(QRectF(-6, 9, 20, 20), 0, 47 * 16); });
         }},
        // Editing.
        {"undo_sketch",
         [](const Pens& p) {
             QPainterPath path;
             path.moveTo(9, 7);
             path.lineTo(4, 12);
             path.lineTo(9, 17);
             p.painter.drawPath(path);
             QPainterPath tail;
             tail.moveTo(4, 12);
             tail.lineTo(15, 12);
             tail.arcTo(QRectF(10, 12, 10, 10), 90, -180);
             tail.lineTo(12, 22);
             p.painter.drawPath(tail);
         }},
        {"fillet",
         [](const Pens& p) {
             QPainterPath corner;
             corner.moveTo(4, 4);
             corner.lineTo(4, 12);
             corner.arcTo(QRectF(4, 4, 16, 16), 180, 90);
             corner.lineTo(20, 20);
             p.painter.drawPath(corner);
             p.dashed([&] {
                 p.line(4, 12, 4, 20);
                 p.line(4, 20, 12, 20);
             });
         }},
        {"rotate", // S20: a square turned about a point
         [](const Pens& p) {
             p.painter.drawRect(QRectF(4, 10, 9, 9));
             p.accented([&] {
                 p.painter.drawArc(QRectF(6, 3, 15, 15), 0, 120 * 16);
                 p.line(21, 10.5, 21, 6);
                 p.line(21, 10.5, 17.5, 8.5);
             });
         }},
        {"scale", // S20: a small square and its larger image
         [](const Pens& p) {
             p.painter.drawRect(QRectF(3, 13, 7, 7));
             p.accented([&] {
                 p.painter.drawRect(QRectF(3, 4, 17, 16));
                 p.line(10, 13, 18, 6);
             });
         }},
        {"chamfer", // S19: a corner cut by a straight line
         [](const Pens& p) {
             QPainterPath corner;
             corner.moveTo(4, 4);
             corner.lineTo(4, 13);
             corner.lineTo(11, 20);
             corner.lineTo(20, 20);
             p.painter.drawPath(corner);
             p.dashed([&] {
                 p.line(4, 13, 4, 20);
                 p.line(4, 20, 11, 20);
             });
         }},
        {"offset",
         [](const Pens& p) {
             p.painter.drawArc(QRectF(3, 9, 18, 18), 20 * 16, 140 * 16);
             p.accented([&] { p.painter.drawArc(QRectF(7, 13, 10, 10), 20 * 16, 140 * 16); });
         }},
        {"toggle_reference", // S18: a dimension in parentheses
         [](const Pens& p) {
             p.line(6, 14, 18, 14);
             p.line(6, 11, 6, 17);
             p.line(18, 11, 18, 17);
             p.accented([&] {
                 p.painter.drawArc(QRectF(1, 6, 6, 16), 100 * 16, 160 * 16);
                 p.painter.drawArc(QRectF(17, 6, 6, 16), -80 * 16, 160 * 16);
             });
         }},
        {"project", // S17: a body edge dropped onto the sketch
         [](const Pens& p) {
             p.painter.drawRect(QRectF(5, 3, 14, 7));
             p.line(12, 11, 12, 17);
             p.line(9.5, 14.5, 12, 17);
             p.line(14.5, 14.5, 12, 17);
             p.accented([&] { p.line(4, 21, 20, 21); });
         }},
        {"linear_pattern",
         [](const Pens& p) {
             p.painter.drawRect(QRectF(2.5, 9, 5, 6));
             p.accented([&] {
                 p.painter.drawRect(QRectF(9.5, 9, 5, 6));
                 p.painter.drawRect(QRectF(16.5, 9, 5, 6));
             });
         }},
        {"edit_pattern",
         [](const Pens& p) {
             p.painter.drawRect(QRectF(2.5, 9, 5, 6));
             p.painter.drawRect(QRectF(9.5, 9, 5, 6));
             p.accented([&] { p.line(13, 21, 21, 3); });
         }},
        {"dissolve_pattern",
         [](const Pens& p) {
             p.painter.drawRect(QRectF(2.5, 9, 5, 6));
             p.dashed([&] {
                 p.painter.drawRect(QRectF(9.5, 9, 5, 6));
                 p.painter.drawRect(QRectF(16.5, 9, 5, 6));
             });
         }},
        {"circular_pattern",
         [](const Pens& p) {
             p.dashed([&] { p.painter.drawEllipse(QPointF(12, 12), 7, 7); });
             p.painter.drawEllipse(QPointF(12, 5), 2.2, 2.2);
             for (int index = 1; index < 6; ++index) {
                 const double angle = -std::numbers::pi / 2 + index * std::numbers::pi / 3;
                 p.dot(12 + 7 * std::cos(angle), 12 + 7 * std::sin(angle), 2.2);
             }
         }},
        {"mirror",
         [](const Pens& p) {
             p.dashed([&] { p.line(12, 3, 12, 21); });
             QPolygonF left{{QPointF(3, 18), QPointF(9, 18), QPointF(9, 7)}};
             p.painter.drawPolygon(left);
             p.accented([&] {
                 QPolygonF right{{QPointF(21, 18), QPointF(15, 18), QPointF(15, 7)}};
                 p.painter.drawPolygon(right);
             });
         }},
        {"trim",
         [](const Pens& p) {
             p.line(12, 3, 12, 21);
             p.line(3, 12, 12, 12);
             p.dashed([&] { p.line(12, 12, 21, 12); });
             p.accented([&] {
                 p.line(16, 8, 20, 16);
                 p.line(20, 8, 16, 16);
             });
         }},
        {"extend",
         [](const Pens& p) {
             p.line(20, 3, 20, 21);
             p.line(3, 12, 11, 12);
             p.accented([&] {
                 p.line(11, 12, 18, 12);
                 p.arrowHead(18, 12, 0);
             });
         }},
        {"split",
         [](const Pens& p) {
             p.line(3, 17, 10, 13);
             p.line(14, 11, 21, 7);
             p.dot(12, 12, 2.4);
         }},
        {"delete",
         [](const Pens& p) {
             p.line(6, 6, 18, 18);
             p.line(18, 6, 6, 18);
         }},
    };
    return table;
}

QPixmap render(const Drawing& drawing, const QColor& ink, const QColor& accent, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    painter.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    drawing(Pens{painter, ink, accent});
    return pixmap;
}

} // namespace

std::optional<QIcon> sketchIcon(const QString& commandId, const QColor& ink, const QColor& accent) {
    if (!commandId.startsWith(QLatin1String("sketch.")))
        return std::nullopt;
    auto key = commandId.mid(7);
    const bool redo = key == QLatin1String("redo_sketch");
    if (redo)
        key = QStringLiteral("undo_sketch");
    const auto found = drawings().find(key);
    if (found == drawings().end())
        return std::nullopt;
    const Drawing drawing = redo ? Drawing([&](const Pens& p) {
        p.painter.translate(24, 0);
        p.painter.scale(-1, 1);
        found.value()(p);
    })
                                 : found.value();
    QIcon icon;
    QColor disabled = ink;
    disabled.setAlphaF(0.35F);
    for (const int size : {18, 20, 24, 36, 40, 48}) {
        icon.addPixmap(render(drawing, ink, accent, size));
        icon.addPixmap(render(drawing, disabled, disabled, size), QIcon::Disabled);
    }
    return icon;
}

} // namespace voelund::desktop
