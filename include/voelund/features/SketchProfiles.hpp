#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <span>
#include <vector>

namespace voelund::features {

struct SketchProfile final {
    document::SketchProfileId id;
    std::vector<document::SketchElementId> elementIds;
};

[[nodiscard]] std::vector<SketchProfile>
detectClosedProfiles(std::span<const document::SketchElement> elements);

} // namespace voelund::features
