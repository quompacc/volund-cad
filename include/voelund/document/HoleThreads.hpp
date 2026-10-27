// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace voelund::document {

// N09 (agent decision 2026-10-27): ISO metric coarse threads (ISO 261/262) for tapped holes.
// The hole is modelled with the core drill (tap drill) diameter; the thread is a stated
// attribute, not modelled geometry. Values in mm.
struct MetricThread final {
    std::string_view designation; // "M6"
    double nominal;
    double pitch;
    double coreDrill;
};
inline constexpr std::array<MetricThread, 14> metricThreads{{
    {"M1.6", 1.6, 0.35, 1.25},
    {"M2", 2.0, 0.4, 1.6},
    {"M2.5", 2.5, 0.45, 2.05},
    {"M3", 3.0, 0.5, 2.5},
    {"M4", 4.0, 0.7, 3.3},
    {"M5", 5.0, 0.8, 4.2},
    {"M6", 6.0, 1.0, 5.0},
    {"M8", 8.0, 1.25, 6.8},
    {"M10", 10.0, 1.5, 8.5},
    {"M12", 12.0, 1.75, 10.2},
    {"M14", 14.0, 2.0, 12.0},
    {"M16", 16.0, 2.0, 14.0},
    {"M20", 20.0, 2.5, 17.5},
    {"M24", 24.0, 3.0, 21.0},
}};

[[nodiscard]] inline std::optional<MetricThread> metricThread(std::string_view designation) {
    for (const auto& thread : metricThreads)
        if (thread.designation == designation)
            return thread;
    return std::nullopt;
}

} // namespace voelund::document
