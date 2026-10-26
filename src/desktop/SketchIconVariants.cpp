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
