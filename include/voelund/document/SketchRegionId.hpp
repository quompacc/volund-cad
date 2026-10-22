// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <voelund/document/SketchElementId.hpp>
namespace voelund::document {
// One source piece of a positively oriented region boundary. Curves with an intrinsic
// counter-clockwise parameter (circle, arc, ellipse) record the traversal direction;
// lines and rectangle sides never do, so their drawing direction cannot change identity.
struct SketchRegionBoundaryUse final {
    SketchElementId element;
    unsigned part; // Rectangle side 0..3, otherwise 0.
    std::optional<bool> reversed;

    [[nodiscard]] auto operator<=>(const SketchRegionBoundaryUse& other) const noexcept {
        return std::tuple{element.value(), part, reversed} <=>
               std::tuple{other.element.value(), other.part, other.reversed};
    }
    [[nodiscard]] bool operator==(const SketchRegionBoundaryUse&) const noexcept = default;
};
// Persistent region identity (R09): the canonical boundary signature
// "o:<outer>;h:<hole>..." built from stable element references, never from indices.
class SketchRegionId final {
  public:
    [[nodiscard]] static SketchRegionId
    fromBoundaries(std::vector<SketchRegionBoundaryUse> outer,
                   std::vector<std::vector<SketchRegionBoundaryUse>> holes);
    // Accepts only canonical text; std::nullopt for anything else.
    [[nodiscard]] static std::optional<SketchRegionId> parse(std::string_view text);

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::vector<SketchElementId> elements() const;
    [[nodiscard]] auto operator<=>(const SketchRegionId&) const noexcept = default;

  private:
    explicit SketchRegionId(std::string text) : text_(std::move(text)) {}
    std::string text_;
};
} // namespace voelund::document
