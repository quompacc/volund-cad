#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <set>

namespace voelund::desktop {

// U06: what the model tree's eye toggles show. View state only: no undo step, no change to
// the document's saved state (ADR 0012).
struct ViewVisibility final {
    // Document origin; empty = automatic, shown only while the document has no body.
    std::optional<bool> origin;
    std::array<bool, 3> planes{true, true, true}; // XY, XZ, YZ under the origin
    bool grid{true};
    std::set<std::uint64_t> hiddenBodies;
    std::set<std::uint64_t> shownBodyOrigins; // body origins start hidden

    [[nodiscard]] bool originShown(bool hasBodies) const noexcept {
        return origin.value_or(!hasBodies);
    }
    // A plane is drawn when its eye is on and the document origin or any shown body origin
    // carries it (bodies have no placement of their own yet).
    [[nodiscard]] std::array<bool, 3> visiblePlanes(bool hasBodies) const noexcept {
        const bool carried = originShown(hasBodies) || !shownBodyOrigins.empty();
        return {carried && planes[0], carried && planes[1], carried && planes[2]};
    }
};

} // namespace voelund::desktop
