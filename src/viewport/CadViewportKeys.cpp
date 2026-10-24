// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"

#include "ViewportPixels.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QKeyEvent>
#include <QMouseEvent>

// D01 (user report 2026-10-23, as in Fusion and Shapr3D): while drawing, the fields at the
// pointer take the keyboard. Digits and a sign or decimal point go into the current field
// without a click, Tab moves to the next, Backspace erases, Enter places the element with the
// typed values. Letters stay tool shortcuts unless an expression ("=Breite*2") is being typed.
namespace voelund::viewport {
namespace {
bool isReturn(int key) { return key == Qt::Key_Return || key == Qt::Key_Enter; }

// W07: a shape's bounding box on screen (framebuffer pixels); empty for an empty shape.
QRect screenBounds(const TopoDS_Shape& shape, const occ::handle<V3d_View>& view) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (box.IsVoid())
        return {};
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    box.Get(x0, y0, z0, x1, y1, z1);
    QRect onScreen;
    for (const double x : {x0, x1})
        for (const double y : {y0, y1})
            for (const double z : {z0, z1}) {
                int px = 0, py = 0;
                view->Convert(x, y, z, px, py);
                onScreen = onScreen.united(QRect(px, py, 1, 1));
            }
    return onScreen;
}
} // namespace

bool CadViewport::takesDynamicKey(const QKeyEvent& event) const {
    const auto& input = impl_->sketchEditor.dynamicInput();
    if (!impl_->sketchEditor.isActive() || !input.active() ||
        (event.modifiers() & (Qt::ControlModifier | Qt::AltModifier)) != 0)
        return false;
    const int key = event.key();
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Backspace)
        return true;
    if (isReturn(key))
        return input.hasTyped();
    const QString text = event.text();
    if (text.size() != 1)
        return false;
    // Ask the field without changing it: a copy takes the character or not.
    auto probe = input;
    return probe.type(text.front().toLatin1());
}

bool CadViewport::handleDynamicKey(const QKeyEvent& event) {
    if (!takesDynamicKey(event))
        return false;
    auto& input = impl_->sketchEditor.dynamicInput();
    const int key = event.key();
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab)
        input.next();
    else if (key == Qt::Key_Backspace)
        input.erase();
    else if (isReturn(key)) {
        // An invalid entry keeps the fields open; the element waits for a value that works.
        if (const auto intent = impl_->sketchEditor.commitTyped();
            intent && impl_->sketchEditorElementHandler)
            impl_->sketchEditorElementHandler(*intent);
    } else
        input.type(event.text().front().toLatin1());
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        const QPoint pointer = impl_->sketchPointer;
        if (impl_->sketchEditorPreviewHandler)
            impl_->sketchEditorPreviewHandler(impl_->sketchEditor.move(pointer.x(), pointer.y()));
        impl_->showDynamicFields(QPointF(pointer) / devicePixelRatioF());
        update();
    });
    return true;
}

void CadViewport::setBodyBoxSelectionHandler(
    std::function<void(const std::vector<std::uint64_t>&, bool additive)> handler) {
    impl_->bodyBoxSelectionHandler = std::move(handler);
}

// W07: a press on empty space outside a sketch starts a body box (after the press handling
// found nothing under the pointer); a click without a drag stays a click on nothing.
void CadViewport::startBodyBox(const QMouseEvent& event) {
    if (!impl_->ready || event.button() != Qt::LeftButton || impl_->sketchEditor.isActive() ||
        impl_->gesture != Impl::Gesture::none || impl_->context->HasDetected() ||
        !impl_->bodyBoxSelectionHandler)
        return;
    impl_->gesture = Impl::Gesture::selectBox;
    impl_->bodyBox = true;
    impl_->gestureOrigin = framebufferPoint(*this, event.position());
    impl_->selectionBand->setCrossing(false);
    impl_->selectionBand->setGeometry(QRect(event.position().toPoint(), QSize{}));
    impl_->selectionBand->show();
}

bool CadViewport::finishBodyBox(const QMouseEvent& event) {
    if (!impl_->bodyBox || event.button() != Qt::LeftButton)
        return false;
    impl_->bodyBox = false;
    impl_->gesture = Impl::Gesture::none;
    impl_->selectionBand->hide();
    const QPoint position = framebufferPoint(*this, event.position());
    const QRect rectangle = QRect(impl_->gestureOrigin, position).normalized();
    if (rectangle.width() < 4 && rectangle.height() < 4)
        return false; // a click: handled as one
    const bool crossing = position.x() < impl_->gestureOrigin.x();
    std::vector<std::uint64_t> ids;
    for (const auto& presentation : impl_->presentations) {
        const QRect onScreen = screenBounds(presentation.shape->Shape(), impl_->view);
        if (!onScreen.isNull() &&
            (crossing ? rectangle.intersects(onScreen) : rectangle.contains(onScreen)))
            ids.push_back(presentation.id);
    }
    impl_->bodyBoxSelectionHandler(ids, event.modifiers().testFlag(Qt::ControlModifier));
    update();
    return true;
}

bool CadViewport::event(QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease &&
        finishBodyBox(*static_cast<QMouseEvent*>(event)))
        return true;
    if (event->type() == QEvent::MouseButtonPress) {
        const bool handled = CadViewportBase::event(event);
        startBodyBox(*static_cast<QMouseEvent*>(event));
        return handled;
    }
    // Tab would move the focus and the window's shortcuts (Return ends a polyline) would take
    // the keys before keyPressEvent; while the fields are open they get them.
    if (event->type() == QEvent::ShortcutOverride &&
        takesDynamicKey(*static_cast<QKeyEvent*>(event))) {
        event->accept();
        return true;
    }
    if (event->type() == QEvent::KeyPress && handleDynamicKey(*static_cast<QKeyEvent*>(event)))
        return true;
    return CadViewportBase::event(event);
}

void CadViewport::setSketchDynamicInputEvaluator(SketchDynamicInput::Evaluator evaluator) {
    impl_->sketchEditor.dynamicInput().setEvaluator(std::move(evaluator));
}

} // namespace voelund::viewport
