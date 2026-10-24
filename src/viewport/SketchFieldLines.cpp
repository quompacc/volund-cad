// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorInteraction.hpp"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

// W12b (user report 2026-10-24, as in FreeCAD and Fusion): while drawing, the typed values sit
// on dimension lines of the preview, not in boxes at the pointer: a rectangle's width below
// it and its height left of it, a line's length along it, a circle's diameter across it.
namespace voelund::viewport {
namespace {
using Point = std::pair<double, double>;
}

std::optional<Point> SketchEditorInteraction::screenPoint(double x, double y) const {
    if (!active_ || view_.IsNull())
        return std::nullopt;
    const gp_Vec xDirection(support_.xDirectionX, support_.xDirectionY, support_.xDirectionZ);
    const gp_Vec normal(support_.normalX, support_.normalY, support_.normalZ);
    const gp_Vec yDirection = normal.Crossed(xDirection);
    const gp_Pnt world = gp_Pnt(support_.originX, support_.originY, support_.originZ)
                             .Translated(xDirection * x + yDirection * y);
    int px = 0, py = 0;
    view_->Convert(world.X(), world.Y(), world.Z(), px, py);
    return Point{static_cast<double>(px), static_cast<double>(py)};
}

// A sketch segment as a screen line; `outward` is the sketch direction it stands off to.
std::optional<SketchEditorInteraction::FieldLine>
SketchEditorInteraction::screenLine(Point from, Point to, Point outward) const {
    const auto a = screenPoint(from.first, from.second);
    const auto b = screenPoint(to.first, to.second);
    const auto c = screenPoint(from.first + outward.first, from.second + outward.second);
    if (!a || !b || !c)
        return std::nullopt;
    const double dx = c->first - a->first, dy = c->second - a->second;
    const double length = std::hypot(dx, dy);
    return FieldLine{*a, *b, length > 1e-9 ? Point{dx / length, dy / length} : Point{0.0, 0.0}};
}

// The dimension line of one field of the preview from `first` to `second`.
std::optional<SketchEditorInteraction::FieldLine>
SketchEditorInteraction::fieldLine(DynamicField kind, Point first, Point second) const {
    const Point low{std::min(first.first, second.first), std::min(first.second, second.second)};
    const Point high{std::max(first.first, second.first), std::max(first.second, second.second)};
    const bool line = tool_ == SketchElementTool::line || tool_ == SketchElementTool::polyline;
    switch (kind) {
    case DynamicField::width:
        return screenLine(low, {high.first, low.second}, {0.0, -1.0});
    case DynamicField::height:
        return screenLine(low, {low.first, high.second}, {-1.0, 0.0});
    case DynamicField::length:
        if (!line)
            return std::nullopt;
        return screenLine(first, second,
                          {first.second - second.second, second.first - first.first});
    case DynamicField::diameter:
        return screenLine({2 * first.first - second.first, 2 * first.second - second.second},
                          second, {0.0, 0.0});
    default:
        return std::nullopt;
    }
}

std::vector<std::optional<SketchEditorInteraction::FieldLine>>
SketchEditorInteraction::fieldLines(int x, int y) const {
    std::vector<std::optional<FieldLine>> lines(dynamic_.fields().size());
    const auto intent = move(x, y);
    if (!intent || intent->third.has_value())
        return lines;
    Point first{intent->firstX, intent->firstY};
    const Point second{intent->secondX, intent->secondY};
    if (tool_ == SketchElementTool::centerRectangle)
        first = {2 * first.first - second.first, 2 * first.second - second.second};
    for (std::size_t index = 0; index < lines.size(); ++index)
        lines[index] = fieldLine(dynamic_.fields()[index].kind, first, second);
    return lines;
}

} // namespace voelund::viewport
