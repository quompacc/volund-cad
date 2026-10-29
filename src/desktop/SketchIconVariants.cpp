// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchIconPens.hpp"

namespace voelund::desktop::detail {

const QHash<QString, IconDrawing>& variantDrawings() {
    static const QHash<QString, IconDrawing> table{
        {"tool.circle_two_point",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 8, 8);
             p.dashed([&] { p.line(4, 12, 20, 12); });
             p.dot(4, 12);
             p.dot(20, 12);
         }},
        {"tool.three_point_rectangle",
         [](const Pens& p) {
             const QPointF corners[]{{4, 15}, {13, 6}, {20, 13}, {11, 22}};
             p.painter.drawPolygon(corners, 4);
             p.dot(4, 15);
             p.dot(13, 6);
             p.dot(20, 13);
         }},
        {"section", // N15a: a body cut by the sketch plane, its outline drawn
         [](const Pens& p) {
             p.painter.drawRect(QRectF(6, 3, 12, 18));
             p.accented([&] {
                 p.line(2, 12, 22, 12);
                 p.painter.drawEllipse(QPointF(12, 12), 2.5, 2.5);
             });
         }},
        {"tool.spline_fit", // N14b: a smooth curve through its points
         [](const Pens& p) {
             QPainterPath curve;
             curve.moveTo(3, 18);
             curve.cubicTo(6, 4, 12, 4, 13, 12);
             curve.cubicTo(14, 20, 19, 20, 21, 7);
             p.painter.drawPath(curve);
             p.dot(3, 18);
             p.dot(13, 12);
             p.dot(21, 7);
         }},
        {"tool.tangent_arc",
         [](const Pens& p) {
             p.line(3, 18, 11, 18);
             p.painter.drawArc(QRectF(5, 6, 12, 12), -90 * 16, 180 * 16);
             p.dot(11, 18);
             p.dot(11, 6, 1.6);
         }},
        {"tool.circle_three_point",
         [](const Pens& p) {
             p.painter.drawEllipse(QPointF(12, 12), 8, 8);
             p.dot(12, 4);
             p.dot(5.1, 16);
             p.dot(18.9, 16);
         }},
    };
    return table;
}

} // namespace voelund::desktop::detail
