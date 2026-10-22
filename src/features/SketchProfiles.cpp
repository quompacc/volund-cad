// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <voelund/features/SketchContourGraph.hpp>
#include <voelund/features/SketchProfiles.hpp>
namespace voelund::features {
std::vector<SketchProfile> detectClosedProfiles(std::span<const document::SketchElement> elements) {
    const auto graph = buildSketchContourGraph(elements);
    std::vector<SketchProfile> profiles;
    for (const auto& loop : graph.loops) {
        SketchProfile profile{
            document::SketchProfileId{graph.pieces[loop.boundary.front().piece].element.value()},
            {},
            {}};
        for (const auto& edge : loop.boundary) {
            const auto id = graph.pieces[edge.piece].element;
            if (std::ranges::find(profile.elementIds, id) == profile.elementIds.end()) {
                profile.elementIds.push_back(id);
                profile.reversed.push_back(edge.reversed);
            }
            if (id.value() < profile.id.value())
                profile.id = document::SketchProfileId{id.value()};
        }
        profiles.push_back(std::move(profile));
    }
    // Compatibility only: persistent region identity is the separate R09 contract.
    std::ranges::sort(profiles, {}, [](const auto& p) { return p.id.value(); });
    return profiles;
}
} // namespace voelund::features
