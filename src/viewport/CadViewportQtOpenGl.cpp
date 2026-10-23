// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadViewportQtOpenGlBackend.hpp"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <stdexcept>

namespace voelund::viewport {

void CadViewport::cleanupGlResources() noexcept {
    if (impl_->cleaningUp)
        return;
    QObject::disconnect(impl_->contextConnection);
    impl_->contextConnection = {};
    if (impl_->graphicDriver.IsNull() && impl_->view.IsNull())
        return;
    impl_->cleaningUp = true;
    ++impl_->cleanupGeneration;
    const bool wasReady = impl_->ready;
    impl_->ready = false;
    try {
        if (wasReady && !impl_->view.IsNull())
            impl_->savedCamera = new Graphic3d_Camera(impl_->view->Camera());
        if (context() && context()->isValid() && context() == impl_->boundContext)
            makeCurrent();
        const bool ownsCurrent = context() && context() == impl_->boundContext &&
                                 context() == QOpenGLContext::currentContext();
        impl_->sketchEditor.rebindView({});
        impl_->sketchEditor.finishElementDrag();
        impl_->gesture = Impl::Gesture::none;
        impl_->selectionBand->hide();
        unsetCursor();
        impl_->manipulator.resetPresentation();
        impl_->pushPull.reset();
        impl_->section.reset();
        if (ownsCurrent && !impl_->context.IsNull())
            impl_->context->RemoveAll(false);
        impl_->sketchPresentation.clear({});
        impl_->sketchPresentation.setReferences({}, {}, false);
        impl_->sketchConstraintPresentation.clear({});
        impl_->presentations.clear();
        if (ownsCurrent && !impl_->view.IsNull())
            impl_->view->Remove();
    } catch (const Standard_Failure& error) {
        qWarning() << "Viewport cleanup failed:" << kernel::occtFailureDiagnostic(error).c_str();
    } catch (const std::exception& error) {
        qWarning() << "Viewport cleanup failed:" << error.what();
    }
    impl_->context.Nullify();
    impl_->view.Nullify();
    impl_->viewer.Nullify();
    impl_->graphicDriver.Nullify();
    impl_->boundContext = nullptr;
    impl_->cleaningUp = false;
    if (context() && context() == QOpenGLContext::currentContext())
        doneCurrent();
}

void CadViewport::initializeGL() {
    auto* current = QOpenGLContext::currentContext();
    if (!current || current != context()) {
        impl_->ready = false;
        return;
    }
    try {
        if (impl_->boundContext != current || impl_->view.IsNull()) {
            cleanupGlResources();
            makeCurrent();
            impl_->createScene();
            impl_->boundContext = current;
            impl_->contextConnection = QObject::connect(
                current, &QOpenGLContext::aboutToBeDestroyed, this,
                [this] { cleanupGlResources(); }, Qt::DirectConnection);
        }
        if (std::exchange(impl_->failNextInitialization, false))
            throw std::runtime_error("P02 injected initialization failure");
        const auto fallback = static_cast<std::uintptr_t>(effectiveWinId());
        impl_->ready = qtgl::attachCurrentContext(
            impl_->view, qtgl::currentNativeWindow(fallback), qRound(width() * devicePixelRatioF()),
            qRound(height() * devicePixelRatioF()), devicePixelRatioF());
        if (!impl_->ready)
            throw std::runtime_error("OCCT context attachment rejected");
        ++impl_->contextGeneration;
        impl_->lastError.clear();
        impl_->displayPendingBodies();
        impl_->sketchEditor.rebindView(impl_->view);
        if (!impl_->savedCamera.IsNull()) {
            impl_->view->SetCamera(impl_->savedCamera);
            // A restored camera may belong to a differently shaped old window.
            impl_->view->MustBeResized();
        }
    } catch (const Standard_Failure& error) {
        impl_->lastError = kernel::occtFailureDiagnostic(error);
        cleanupGlResources();
        qWarning() << "Viewport initialization failed:" << impl_->lastError.c_str();
    } catch (const std::exception& error) {
        impl_->lastError = error.what();
        cleanupGlResources();
        qWarning() << "Viewport initialization failed:" << impl_->lastError.c_str();
    }
}

void CadViewport::paintGL() {
    if (!impl_->ready)
        return;
    try {
        if (QOpenGLContext::currentContext() != impl_->boundContext)
            throw std::runtime_error("Viewport context unavailable");
        // Qt allocates its FBO using rounded QSize scaling, but its default
        // glViewport truncates fractional dimensions. OCCT wraps that viewport.
        const QSize pixels = size() * devicePixelRatioF();
        context()->functions()->glViewport(0, 0, pixels.width(), pixels.height());
        if (!qtgl::bindCurrentFramebuffer(impl_->view, devicePixelRatioF()))
            throw std::runtime_error("Viewport framebuffer/context unavailable");
        qtgl::resetStateBeforeOcct(impl_->view);
        impl_->view->Redraw();
        qtgl::resetStateAfterOcct(impl_->view);
        ++impl_->renderedFrames;
    } catch (const Standard_Failure& error) {
        impl_->ready = false;
        impl_->lastError = kernel::occtFailureDiagnostic(error);
        qWarning() << "Viewport rendering failed:" << impl_->lastError.c_str();
    } catch (const std::exception& error) {
        impl_->ready = false;
        impl_->lastError = error.what();
        qWarning() << "Viewport rendering failed:" << error.what();
    }
}

void CadViewport::resizeGL(int, int) {
    // Binding the current FBO in paintGL propagates its actual dimensions.
    update();
}
} // namespace voelund::viewport
