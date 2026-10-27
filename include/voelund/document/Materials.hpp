// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace voelund::document {

// N07 (agent decision 2026-10-27): the materials a body can be given, with a typical density
// in g/cm³. Kept small; a new entry needs only a line here (the key is stored, with the
// density). No strengths: those belong to FEM (FEM01).
struct MaterialPreset final {
    std::string_view key;
    std::string_view name; // German, as shown
    double density;
};
inline constexpr std::array<MaterialPreset, 12> materialPresets{{
    {"steel", "Stahl", 7.85},
    {"stainless", "Edelstahl", 7.93},
    {"cast_iron", "Gusseisen", 7.20},
    {"aluminium", "Aluminium", 2.70},
    {"brass", "Messing", 8.50},
    {"copper", "Kupfer", 8.96},
    {"titanium", "Titan", 4.51},
    {"pla", "PLA", 1.24},
    {"abs", "ABS", 1.04},
    {"petg", "PETG", 1.27},
    {"nylon", "Nylon (PA12)", 1.01},
    {"wood", "Holz (Buche)", 0.72},
}};
inline constexpr std::string_view customMaterialKey = "custom";

[[nodiscard]] inline std::optional<MaterialPreset> materialPreset(std::string_view key) {
    for (const auto& preset : materialPresets)
        if (preset.key == key)
            return preset;
    return std::nullopt;
}

} // namespace voelund::document
