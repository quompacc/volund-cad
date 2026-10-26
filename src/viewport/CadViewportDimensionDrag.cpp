// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"
#include "ViewportPixels.hpp"

#include <QMouseEvent>

namespace voelund::viewport {

void CadViewport::setSketchDimensionDragHandler(
    std::function<void(std::uint64_t, double, double, bool)> handler) {
    impl_->sketchDimensionDragHandler = std::move(handler);
}

// W21 (as SolidCraft): a press on a dimension's value notes it (the press still selects it).
void CadViewport::noteDimensionPress(const QMouseEvent& mouse, QPoint position) {
    impl_->dimensionDrag = {};
    if (!impl_->ready || mouse.button() != Qt::LeftButton || !impl_->sketchEditor.isActive() ||
        !impl_->sketchDimensionDragHandler)
        return;
    const auto id =
        impl_->sketchConstraintPresentation.constraintAt(impl_->view, position.x(), position.y());
    if (id && impl_->sketchConstraintPresentation.hasDimension(*id))
        impl_->dimensionDrag = {*id, position, false};
}

// Moving more than a few pixels drags the dimension line along; the release ends the drag. A
// plain click stays a click.
bool CadViewport::sketchDimensionDrag(QEvent& event) {
    const auto type = event.type();
    if (type == QEvent::MouseButtonPress) {
        const auto& press = static_cast<const QMouseEvent&>(event);
        noteDimensionPress(press, framebufferPoint(*this, press.position()));
        return false;
    }
    auto& drag = impl_->dimensionDrag;
    if ((type != QEvent::MouseMove && type != QEvent::MouseButtonRelease) || drag.id == 0)
        return false;
    const auto& mouse = static_cast<const QMouseEvent&>(event);
    const QPoint position = framebufferPoint(*this, mouse.position());
    const bool finished = type == QEvent::MouseButtonRelease;
    if (!drag.moving &&
        (position - drag.origin).manhattanLength() <= qRound(4 * devicePixelRatioF())) {
        if (finished)
            drag = {};
        return false;
    }
    drag.moving = true;
    impl_->gesture = Impl::Gesture::none;
    if (const auto local = impl_->sketchEditor.projectPoint(position.x(), position.y()))
        impl_->sketchDimensionDragHandler(drag.id, local->first, local->second, finished);
    if (finished)
        drag = {};
    update();
    return true;
}

} // namespace voelund::viewport
