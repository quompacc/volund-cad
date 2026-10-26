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
