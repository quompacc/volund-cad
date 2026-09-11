#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <Aspect_Drawable.hxx>
#include <V3d_View.hxx>

#include <cstdint>

namespace voelund::viewport::qtgl {

[[nodiscard]] Aspect_Drawable currentNativeWindow(std::uintptr_t fallbackWindowId);

[[nodiscard]] bool attachCurrentContext(
    const occ::handle<V3d_View>& view,
    Aspect_Drawable nativeWindow,
    int width,
    int height,
    double devicePixelRatio);

[[nodiscard]] bool bindCurrentFramebuffer(const occ::handle<V3d_View>& view);
void resetStateBeforeOcct(const occ::handle<V3d_View>& view);
void resetStateAfterOcct(const occ::handle<V3d_View>& view);

} // namespace voelund::viewport::qtgl
