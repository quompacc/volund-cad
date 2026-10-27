// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>

#include <array>
#include <optional>

namespace voelund::kernel {

// M12: what one picked edge, face or solid measures, in mm, mm² and mm³ (full precision; the
// display rounds). Absent values do not apply to the shape.
struct ShapeMeasure final {
    std::optional<double> length{};
    std::optional<double> radius{}; // circular edge, cylinder, cone base or sphere
    std::optional<double> area{};
    std::optional<double> volume{};
    std::optional<std::array<double, 3>> point{}; // N08: a vertex
};
[[nodiscard]] ShapeMeasure measureShape(const TopoDS_Shape& shape);

// M12: the shortest distance between two shapes, and the angle (radians, 0 to pi/2) between
// them when both have a direction: a straight edge (its line) or a planar face (its normal).
struct PairMeasure final {
    double distance;
    std::optional<double> angle{};
    std::optional<std::array<double, 3>> delta{}; // N08: second minus first, two vertices
};
[[nodiscard]] PairMeasure measureBetween(const TopoDS_Shape& first, const TopoDS_Shape& second);

// N08: a point of an edge to measure from, as a vertex: its middle (half its length along the
// curve) or the centre of a circular edge; null when the edge has no such point.
enum class EdgePoint { middle, center };
[[nodiscard]] TopoDS_Shape edgePoint(const TopoDS_Shape& edge, EdgePoint which);

} // namespace voelund::kernel
