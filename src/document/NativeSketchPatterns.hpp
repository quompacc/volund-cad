// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/SketchPattern.hpp>

#include <cstddef>

// S12 (schema 11): sketch child 14, written only for a sketch with patterns.
//   Integer = pattern count; child 1 = next pattern ID (text);
//   child 1 + i = pattern i: RealArray(5) step x/y, center x/y, angle;
//     children 1 ID (text), 2 kind (Integer), 3 count (Integer), 4 axis ID (text, 0 = none),
//     5 sources, 6 instances (each Integer = n and n children with an element ID as text).
namespace voelund::document::detail {
// Records a sketch's patterns take from the document budget.
[[nodiscard]] std::size_t nativeSketchPatternRecords(const SketchPatterns& patterns) noexcept;
void writeNativeSketchPatterns(const TDF_Label& label, const SketchPatterns& patterns);
[[nodiscard]] SketchPatterns readNativeSketchPatterns(const TDF_Label& label,
                                                      std::size_t remainingRecords);
} // namespace voelund::document::detail
