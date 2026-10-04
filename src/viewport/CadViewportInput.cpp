#include "ViewportContextGuard.hpp"
#include "ViewportPixels.hpp"
#include <QDebug>
#include <stdexcept>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

namespace voelund::viewport {

void CadViewport::mouseMoveEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready) {
            return;
        }

        const QPoint position = framebufferPoint(*this, event->position());
        if (impl_->gesture == Impl::Gesture::selectBox) {
            impl_->selectionBand->setGeometry(
                logicalRectangle(*this, impl_->gestureOrigin, position));
            return;
        }
        if (impl_->sketchEditor.isElementDragging() && event->buttons().testFlag(Qt::LeftButton)) {
            const auto delta = impl_->sketchEditor.dragElement(position.x(), position.y());
            if (delta.has_value() && impl_->sketchEditorTranslationHandler) {
                impl_->sketchEditorTranslationHandler(delta->first, delta->second);
            }
            update();
            return;
        }
        if (impl_->sketch.isActive() && event->buttons() == Qt::NoButton) {
            impl_->sketch.move(position.x(), position.y());
            update();
            return;
        }
        if (impl_->sketchEditor.isActive() && event->buttons() == Qt::NoButton) {
            if (impl_->sketchEditorPreviewHandler) {
                impl_->sketchEditorPreviewHandler(
                    impl_->sketchEditor.move(position.x(), position.y()));
            }
            update();
            return;
        }
        if (impl_->gesture == Impl::Gesture::rotate) {
            impl_->view->Rotation(position.x(), position.y());
        } else if (impl_->gesture == Impl::Gesture::pan) {
            const QPoint delta = position - impl_->gestureOrigin;
            impl_->view->Pan(delta.x(), -delta.y(), 1.0, false);
        } else if (impl_->gesture == Impl::Gesture::moveAxis) {
            impl_->manipulator.update(position.x(), position.y(), impl_->view,
                                      event->modifiers().testFlag(Qt::ControlModifier));
        } else if (impl_->gesture != Impl::Gesture::selectBox) {
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
        }
        update();
    });
}

void CadViewport::mousePressEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready) {
            return;
        }

        impl_->gestureOrigin = framebufferPoint(*this, event->position());
        if (event->button() == Qt::RightButton) {
            impl_->gesture = Impl::Gesture::rotate;
            impl_->view->StartRotation(impl_->gestureOrigin.x(), impl_->gestureOrigin.y());
            setCursor(Qt::ClosedHandCursor);
        } else if (event->button() == Qt::MiddleButton) {
            impl_->gesture = Impl::Gesture::pan;
            impl_->view->Pan(0, 0, 1.0, true);
            setCursor(Qt::SizeAllCursor);
        } else if (event->button() == Qt::LeftButton) {
            if (impl_->sketch.isActive()) {
                return;
            }
            const QPoint position = framebufferPoint(*this, event->position());
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            if (impl_->sketchEditor.isActive()) {
                impl_->sketchElementPressed = false;
                if (impl_->sketchEditor.isDrawing())
                    return;
                if (impl_->context->HasDetected()) {
                    const auto detected = impl_->context->DetectedInteractive();
                    if (impl_->sketchConstraintPresentation.detectedConstraint(detected))
                        return;
                    if (const auto id = impl_->sketchPresentation.detectedElement(detected)) {
                        const bool additive = event->modifiers().testFlag(Qt::ControlModifier);
                        impl_->sketchElementPressed = !additive;
                        if (!additive)
                            static_cast<void>(
                                impl_->sketchEditor.beginElementDrag(position.x(), position.y()));
                        if (impl_->sketchElementSelectionHandler) {
                            const auto local =
                                impl_->sketchEditor.projectPoint(position.x(), position.y());
                            if (local.has_value())
                                impl_->sketchElementSelectionHandler(
                                    {*id, local->first, local->second, additive});
                        }
                        update();
                    }
                } else {
                    impl_->gesture = Impl::Gesture::selectBox;
                    impl_->selectionBand->setGeometry(QRect(event->position().toPoint(), QSize{}));
                    impl_->selectionBand->show();
                }
                return;
            }
            if (!impl_->context->HasDetected()) {
                return;
            }
            const occ::handle<AIS_InteractiveObject> detected =
                impl_->context->DetectedInteractive();
            if (impl_->manipulator.matches(detected) && impl_->manipulator.hasActiveMode()) {
                impl_->gesture = Impl::Gesture::moveAxis;
                impl_->manipulator.start(position.x(), position.y(), impl_->view);
                setCursor(Qt::ClosedHandCursor);
                return;
            }
            if (const auto elementId = impl_->sketchPresentation.detectedElement(detected)) {
                if (impl_->sketchElementSelectionHandler) {
                    impl_->sketchElementSelectionHandler(
                        {*elementId, 0.0, 0.0, event->modifiers().testFlag(Qt::ControlModifier)});
                }
                update();
                return;
            }
            impl_->notifyDetectedBody(false);
        }
    });
}

void CadViewport::mouseDoubleClickEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready || event->button() != Qt::LeftButton)
            return;
        const QPoint position = framebufferPoint(*this, event->position());
        impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
        if (!impl_->context->HasDetected())
            return;
        const auto id = impl_->sketchConstraintPresentation.detectedConstraint(
            impl_->context->DetectedInteractive());
        if (id.has_value() && impl_->sketchConstraintEditHandler)
            impl_->sketchConstraintEditHandler(*id);
    });
}

void CadViewport::mouseReleaseEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (impl_->ready && event->button() == Qt::LeftButton &&
            impl_->gesture == Impl::Gesture::moveAxis) {
            impl_->manipulator.finish();
            impl_->gesture = Impl::Gesture::none;
            unsetCursor();
            update();
            return;
        }
        if (impl_->ready && event->button() == Qt::LeftButton) {
            const QPoint position = framebufferPoint(*this, event->position());
            if (impl_->sketch.isActive()) {
                const auto rectangle = impl_->sketch.click(position.x(), position.y());
                if (rectangle.has_value() && impl_->sketchRectangleHandler) {
                    impl_->sketchRectangleHandler(*rectangle);
                }
                update();
                return;
            }
            if (impl_->sketchEditor.isActive()) {
                if (impl_->gesture == Impl::Gesture::selectBox) {
                    const QRect rectangle(impl_->gestureOrigin, position);
                    impl_->gesture = Impl::Gesture::none;
                    impl_->selectionBand->hide();
                    if (impl_->sketchElementSelectionHandler)
                        for (const auto& selection : boxSelectionIntents(
                                 impl_->pendingSketchElements, impl_->view, rectangle.normalized()))
                            impl_->sketchElementSelectionHandler(selection);
                    update();
                    return;
                }
                if (impl_->sketchElementPressed) {
                    impl_->sketchElementPressed = false;
                    impl_->sketchEditor.finishElementDrag();
                    return;
                }
                const auto intent = impl_->sketchEditor.isDrawing()
                                        ? impl_->sketchEditor.click(position.x(), position.y())
                                        : std::nullopt;
                if (intent.has_value() && impl_->sketchEditorElementHandler) {
                    impl_->sketchEditorElementHandler(*intent);
                }
                if (impl_->sketchEditorPreviewHandler) {
                    impl_->sketchEditorPreviewHandler(std::nullopt);
                }
                update();
                return;
            }
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            if (impl_->context->HasDetected()) {
                const occ::handle<AIS_InteractiveObject> detected =
                    impl_->context->DetectedInteractive();
                if (impl_->sketchPresentation.detectedElement(detected).has_value()) {
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                impl_->notifyDetectedBody(true);
            }
            // A click selects only the topmost face. SelectPoint would select every owner the
            // pick ray crosses (e.g. a cylinder's cap and its far side), and the callback above
            // may have rebuilt the presentations, so detect again before selecting.
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            if (impl_->context->HasDetected())
                impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
            else
                impl_->context->ClearSelected(false);
            update();
        }

        impl_->gesture = Impl::Gesture::none;
        unsetCursor();
    });
}

void CadViewport::wheelEvent(QWheelEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready) {
            return;
        }

        constexpr double zoomStep = 1.15;
        impl_->view->SetZoom(event->angleDelta().y() > 0 ? zoomStep : 1.0 / zoomStep);
        update();
        event->accept();
    });
}

} // namespace voelund::viewport
