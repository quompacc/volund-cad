#pragma once
#include <QDebug>
#include <exception>
#include <voelund/viewport/CadViewport.hpp>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <QOpenGLContext>
#include <QPointer>
#include <QSurface>
#include <stdexcept>
#endif

namespace voelund::viewport {
// Scope an external scene/input operation to this widget's context. Qt's own
// paint/initialize callbacks already enter with their context current.
class ViewportContextGuard final {
  public:
    explicit ViewportContextGuard(CadViewport& viewport) {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
        if (!viewport.isRenderingReady())
            return;
        previous_ = QOpenGLContext::currentContext();
        if (previous_ == viewport.context())
            return;
        surface_ = previous_ ? previous_->surface() : nullptr;
        viewport.makeCurrent();
        if (QOpenGLContext::currentContext() != viewport.context())
            throw std::runtime_error("Cannot activate viewport context");
        viewport_ = &viewport;
#else
        static_cast<void>(viewport);
#endif
    }
    ~ViewportContextGuard() {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
        if (!viewport_)
            return;
        viewport_->doneCurrent();
        if (previous_ && surface_)
            previous_->makeCurrent(surface_);
#endif
    }

  private:
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    CadViewport* viewport_{nullptr};
    QPointer<QOpenGLContext> previous_;
    QSurface* surface_{nullptr};
#endif
};

template <typename Operation>
void performViewportOperation(CadViewport& viewport, bool& ready, Operation&& operation) {
    try {
        ViewportContextGuard guard(viewport);
        operation();
    } catch (const Standard_Failure& error) {
        ready = false;
        qWarning() << "Viewport operation failed:" << kernel::occtFailureDiagnostic(error).c_str();
    } catch (const std::exception& error) {
        ready = false;
        qWarning() << "Viewport operation failed:" << error.what();
    }
}
} // namespace voelund::viewport
