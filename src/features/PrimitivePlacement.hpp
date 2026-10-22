// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <algorithm>
#include <stdexcept>
#include <voelund/kernel/ShapeFactory.hpp>

namespace voelund::features::detail {
inline kernel::ShapeOperationResult placePrimitive(const kernel::ShapeOperationResult& source,
                                                   double x, double y, double z,
                                                   const kernel::RigidTransform& placement) {
    auto result = kernel::ShapeFactory::applyTransformWithHistory(source.shape, x, y, z, placement);
    for (const auto& origin : source.origins) {
        const auto found = std::ranges::find_if(
            result.history, [&](const auto& item) { return item.source.IsSame(origin.shape); });
        if (found == result.history.end() || found->coverage != kernel::HistoryCoverage::complete ||
            found->modified.size() != 1)
            throw std::logic_error("Rigid primitive placement lost semantic face provenance");
        // S03: keep the source orientation. A rotation can return the moved face without
        // it; its frame (frameFace) would then face the other way than in the unplaced
        // reference result, and a bound sketch would flip between the two evaluations.
        result.origins.push_back(
            {origin.role, found->modified.front().Oriented(origin.shape.Orientation())});
    }
    return result;
}
} // namespace voelund::features::detail
