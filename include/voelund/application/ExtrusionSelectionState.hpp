// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <vector>

namespace voelund::application {
struct ExtrusionRegionState final {
    document::SketchRegionId region;
    features::SketchRegionReferenceStatus status;
};
struct ExtrusionSelectionState final {
    std::vector<document::SketchProfileId> legacyProfiles; // Evaluated as before; needs repair.
    std::vector<ExtrusionRegionState> regions;
    [[nodiscard]] bool needsRepair() const noexcept;
};
} // namespace voelund::application
