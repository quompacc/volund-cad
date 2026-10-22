// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <cstdint>

namespace voelund::document {

class FeatureId final {
  public:
    constexpr explicit FeatureId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const FeatureId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

} // namespace voelund::document
