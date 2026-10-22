// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/SketchCurveEdit.hpp>

#include <optional>
#include <span>
#include <vector>

// S10: the geometry of trim, extend and split (tool matrix in SketchCurveEdit.hpp). The plan
// changes nothing; anything the edit cannot do throws std::invalid_argument with the reason.
namespace voelund::application::detail {

// A changed end of a piece that lies on a cutting edge.
struct SketchCurveEditBinding final {
    std::size_t piece; // 0: the edited element, 1: the new piece
    document::SketchPointPosition point;
    document::SketchElementId cutter;
    // The cut is the cutter's end point (coincident); otherwise the end lies on the cutter.
    std::optional<document::SketchPointPosition> cutterEnd;
};

struct SketchCurveEditPlan final {
    document::SketchGeometry edited; // the element's new geometry (a trimmed circle is an arc)
    // The second piece; it takes over the element's end point.
    std::optional<document::SketchGeometry> piece;
    // Split: the piece starts where the edited element now ends.
    bool joined{false};
    // The end of the element that moved (trim or extend at one end).
    std::optional<document::SketchPointPosition> movedEnd;
    // Split of a line: the share of the old length the element keeps.
    std::optional<double> keptShare;
    std::vector<SketchCurveEditBinding> bindings;
};

[[nodiscard]] SketchCurveEditPlan planCurveEdit(SketchCurveEdit edit,
                                                std::span<const document::SketchElement> elements,
                                                document::SketchElementId target,
                                                document::SketchPoint pick);

// Whether the edit supports the element's curve type at all.
[[nodiscard]] bool supportsCurveEdit(SketchCurveEdit edit,
                                     const document::SketchGeometry& geometry) noexcept;

// "Stutzen", "Verlängern", "Teilen": the prefix of every message of the edit.
[[nodiscard]] const char* curveEditName(SketchCurveEdit edit) noexcept;

} // namespace voelund::application::detail
