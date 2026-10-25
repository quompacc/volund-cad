// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"

namespace voelund::viewport {

// W15 (user report 2026-10-25): the areas of the selected sketch, lightly filled.
void CadViewport::setSketchRegions(std::span<const SceneSketchRegion> regions) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->pendingSketchRegions.assign(regions.begin(), regions.end());
        if (impl_->ready)
            impl_->sketchRegions.show(impl_->context, impl_->pendingSketchRegions, impl_->theme);
        update();
    });
}

std::size_t CadViewport::displayedSketchRegionCount() const noexcept {
    return impl_->sketchRegions.size();
}

std::optional<std::string> CadViewport::hoveredSketchRegion() const {
    return impl_->sketchRegions.hovered();
}

} // namespace voelund::viewport
