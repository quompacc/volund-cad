// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"

#include <QKeyEvent>

// D01 (user report 2026-10-23, as in Fusion and Shapr3D): while drawing, the fields at the
// pointer take the keyboard. Digits and a sign or decimal point go into the current field
// without a click, Tab moves to the next, Backspace erases, Enter places the element with the
// typed values. Letters stay tool shortcuts unless an expression ("=Breite*2") is being typed.
namespace voelund::viewport {
namespace {
bool isReturn(int key) { return key == Qt::Key_Return || key == Qt::Key_Enter; }
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

bool CadViewport::event(QEvent* event) {
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
