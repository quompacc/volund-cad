// Portions adapted from occt-samples-qt, Copyright (c) 2023 Kirill Gavrilov.
// Used under the MIT License; see docs/provenance/occt-samples-qt.md.

#if defined(_WIN32)
#include <windows.h>
#endif

#include "QtOcctGlContext.hpp"

#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_GlCore20.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_View.hxx>
#include <OpenGl_Window.hxx>

namespace voelund::viewport::qtgl {
namespace {

class NeutralWindow final : public Aspect_NeutralWindow {
    DEFINE_STANDARD_RTTI_INLINE(NeutralWindow, Aspect_NeutralWindow)

public:
    [[nodiscard]] double DevicePixelRatio() const override { return pixelRatio_; }
    void setDevicePixelRatio(double ratio) { pixelRatio_ = ratio; }

private:
    double pixelRatio_{1.0};
};

class QtFrameBuffer final : public OpenGl_FrameBuffer {
    DEFINE_STANDARD_RTTI_INLINE(QtFrameBuffer, OpenGl_FrameBuffer)

public:
    void BindBuffer(const occ::handle<OpenGl_Context>& context) override
    {
        OpenGl_FrameBuffer::BindBuffer(context);
        context->SetFrameBufferSRGB(true, false);
    }

    void BindDrawBuffer(const occ::handle<OpenGl_Context>& context) override
    {
        OpenGl_FrameBuffer::BindDrawBuffer(context);
        context->SetFrameBufferSRGB(true, false);
    }
};

occ::handle<OpenGl_Context> contextFor(const occ::handle<V3d_View>& view)
{
    const auto openGlView = occ::handle<OpenGl_View>::DownCast(view->View());
    return openGlView->GlWindow()->GetGlContext();
}

} // namespace

Aspect_Drawable currentNativeWindow(std::uintptr_t fallbackWindowId)
{
#if defined(_WIN32)
    static_cast<void>(fallbackWindowId);
    const HDC deviceContext = wglGetCurrentDC();
    return reinterpret_cast<Aspect_Drawable>(WindowFromDC(deviceContext));
#else
    return static_cast<Aspect_Drawable>(fallbackWindowId);
#endif
}

bool attachCurrentContext(
    const occ::handle<V3d_View>& view,
    Aspect_Drawable nativeWindow,
    int width,
    int height,
    double devicePixelRatio)
{
    const auto driver = occ::handle<OpenGl_GraphicDriver>::DownCast(view->Viewer()->Driver());
    occ::handle<OpenGl_Context> context = new OpenGl_Context();
    if (!context->Init(!driver->Options().contextCompatible)) {
        return false;
    }

    occ::handle<NeutralWindow> window = new NeutralWindow();
    window->SetVirtual(true);
    window->SetNativeHandle(nativeWindow);
    window->SetSize(width, height);
    window->setDevicePixelRatio(devicePixelRatio);
    view->SetWindow(window, context->RenderingContext());
    view->MustBeResized();
    view->Invalidate();
    return true;
}

bool bindCurrentFramebuffer(const occ::handle<V3d_View>& view)
{
    const auto context = contextFor(view);
    auto frameBuffer = occ::handle<QtFrameBuffer>::DownCast(context->DefaultFrameBuffer());
    if (frameBuffer.IsNull()) {
        frameBuffer = new QtFrameBuffer();
    }
    if (!frameBuffer->InitWrapper(context)) {
        return false;
    }

    context->SetDefaultFrameBuffer({});
    const auto size = frameBuffer->GetVPSize();
    const auto window = occ::handle<NeutralWindow>::DownCast(view->Window());
    int oldWidth = 0;
    int oldHeight = 0;
    window->Size(oldWidth, oldHeight);
    if (oldWidth != size.x() || oldHeight != size.y()) {
        window->SetSize(size.x(), size.y());
        view->MustBeResized();
        view->Invalidate();
    }
    context->SetDefaultFrameBuffer(frameBuffer);
    return true;
}

void resetStateBeforeOcct(const occ::handle<V3d_View>& view)
{
    const auto context = contextFor(view);
    if (context.IsNull()) {
        return;
    }
    if (context->core20fwd != nullptr) {
        context->core20fwd->glUseProgram(0);
    }
    context->core11fwd->glBindTexture(GL_TEXTURE_2D, 0);
    context->core11fwd->glDisable(GL_BLEND);
}

void resetStateAfterOcct(const occ::handle<V3d_View>& view)
{
    const auto context = contextFor(view);
    if (context.IsNull()) {
        return;
    }
    context->core11fwd->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    context->core11fwd->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (context->core15fwd != nullptr) {
        context->core15fwd->glActiveTexture(GL_TEXTURE0);
    }
}

} // namespace voelund::viewport::qtgl
