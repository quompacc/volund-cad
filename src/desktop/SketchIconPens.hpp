// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QColor>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QString>

#include <cmath>
#include <functional>
#include <numbers>

// The pens of the sketch symbols (SketchIcons.cpp, SketchIconVariants.cpp): a 24-unit grid,
// geometry in ink, points and markers in the accent colour.
namespace voelund::desktop::detail {

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

using IconDrawing = std::function<void(const Pens&)>;

// W20: the symbols of the circle variants, merged into the sketch symbols.
[[nodiscard]] const QHash<QString, IconDrawing>& variantDrawings();

} // namespace voelund::desktop::detail
