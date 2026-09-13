#include "CadViewportNativeBackend.hpp"
#include "ViewportContextGuard.hpp"
namespace voelund::viewport {
QPaintEngine* CadViewport::paintEngine() const { return nullptr; }

void CadViewport::paintEvent(QPaintEvent* event) {
    event->accept();
    performViewportOperation(*this, impl_->ready, [&] {
        if (impl_->ready)
            impl_->view->Redraw();
    });
}

void CadViewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    performViewportOperation(*this, impl_->ready, [&] { impl_->resize(); });
}

void CadViewport::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->initialize(static_cast<std::uintptr_t>(winId()));
        impl_->resize();
    });
    QTimer::singleShot(0, this, [this] {
        performViewportOperation(*this, impl_->ready, [&] { impl_->resize(); });
    });
}
} // namespace voelund::viewport
