// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchText.hpp"
#include "Theme.hpp"

#include <voelund/kernel/OutlineUnion.hpp>

#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainterPath>

#include <cmath>

namespace voelund::desktop {
namespace {

// Glyph outlines in font units (y down) to sketch millimetres (y up).
struct Placement final {
    double scale;
    document::SketchPoint origin;
    [[nodiscard]] gp_Pnt2d map(const QPointF& point) const {
        return {origin.x + point.x() * scale, origin.y - point.y() * scale};
    }
};

// The contours of the path: straight and cubic pieces; pieces shorter than `shortest` dropped.
std::vector<kernel::OutlineContour> contoursOf(const QPainterPath& path, const Placement& placement,
                                               double shortest) {
    std::vector<kernel::OutlineContour> contours;
    gp_Pnt2d current;
    for (int index = 0; index < path.elementCount(); ++index) {
        const auto element = path.elementAt(index);
        const gp_Pnt2d point = placement.map(element);
        if (element.isMoveTo()) {
            contours.emplace_back();
        } else if (element.isLineTo() && current.Distance(point) > shortest) {
            contours.back().push_back({true, {current, point, point, point}});
        } else if (element.isCurveTo() && index + 2 < path.elementCount()) {
            const gp_Pnt2d second = placement.map(path.elementAt(index + 1));
            const gp_Pnt2d end = placement.map(path.elementAt(index + 2));
            if (current.Distance(end) > shortest)
                contours.back().push_back({false, {current, point, second, end}});
            current = end;
            index += 2;
            continue;
        } else if (element.isLineTo()) {
            continue; // too short: the contour goes on from where it was
        }
        current = point;
    }
    std::erase_if(contours, [](const kernel::OutlineContour& c) { return c.size() < 2; });
    return contours;
}

document::SketchGeometry sketchCurve(const kernel::OutlinePiece& piece) {
    if (piece.straight)
        return document::SketchLine{{piece.start.X(), piece.start.Y()},
                                    {piece.end.X(), piece.end.Y()}};
    document::SketchBSpline spline;
    for (const auto& pole : piece.spline.poles)
        spline.poles.push_back({pole.X(), pole.Y()});
    // A single Bézier piece keeps the uniform knots; a trimmed one its own.
    if (spline.poles.size() != 4 || piece.spline.knots.back() != 1.0)
        spline.knots = piece.spline.knots;
    return spline;
}

} // namespace

QStringList sketchTextFamilies() {
    QStringList families{uiFontFamily()};
    for (const auto& family : {QFontDatabase::systemFont(QFontDatabase::GeneralFont).family(),
                               QFont(QStringLiteral("Serif")).defaultFamily()})
        if (!families.contains(family))
            families << family;
    return families;
}

std::vector<document::SketchGeometry> sketchTextCurves(const QString& text, const QString& family,
                                                       double height,
                                                       document::SketchPoint origin) {
    QFont font(family);
    font.setPointSizeF(100.0);
    const double capital = QFontMetricsF(font).capHeight();
    if (text.trimmed().isEmpty() || capital <= 0.0 || height <= 0.0)
        return {};
    QPainterPath path;
    path.addText(0.0, 0.0, font, text);
    path.setFillRule(Qt::WindingFill); // as glyphs are filled
    const Placement placement{height / capital, origin};
    // Overlapping glyph contours (as variable fonts draw them) are merged into one outline.
    const auto filled = [&](const gp_Pnt2d& point) {
        return path.contains(QPointF((point.X() - origin.x) / placement.scale,
                                     (origin.y - point.Y()) / placement.scale));
    };
    std::vector<document::SketchGeometry> curves;
    for (const auto& piece :
         kernel::mergedOutline(contoursOf(path, placement, height * 1e-4), filled))
        curves.push_back(sketchCurve(piece));
    return curves;
}

} // namespace voelund::desktop
