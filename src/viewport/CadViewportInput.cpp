// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ViewportContextGuard.hpp"
#include "ViewportPixels.hpp"
#include <QDebug>
#include <QKeyEvent>
#include <StdSelect_ViewerSelector3d.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <optional>
#include <stdexcept>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

namespace voelund::viewport {

std::optional<std::pair<SceneSketchPoint, QPointF>>
CadViewportScene::sketchPointNear(QPoint framebuffer, double ratio) const {
    std::optional<std::pair<SceneSketchPoint, QPointF>> nearest;
    double best = 12.0 * ratio;
    for (const SceneSketchElement& element : pendingSketchElements)
        for (const SceneSketchPoint& point : element.points) {
            // Midpoints are no reference of their own.
            if (point.kind != SketchPointKind::start && point.kind != SketchPointKind::end &&
                point.kind != SketchPointKind::center)
                continue;
            int x = 0, y = 0;
            view->Convert(point.worldX, point.worldY, point.worldZ, x, y);
            const double distance = std::hypot(x - framebuffer.x(), y - framebuffer.y());
            if (distance <= best) {
                best = distance;
                nearest = std::pair{point, QPointF(x, y) / ratio};
            }
        }
    return nearest;
}

void CadViewportScene::hoverSketchPoint(QPoint framebuffer, double ratio) {
    const auto nearby =
        sketchEditor.isDrawing() ? std::nullopt : sketchPointNear(framebuffer, ratio);
    glyphOverlay->setHoveredPoint(nearby ? std::optional{nearby->second} : std::nullopt);
}

// A lit corner is taken even when the pointer is just off its lines; the light goes out, as
// the point may move with the press.
std::optional<std::uint64_t>
CadViewportScene::pressedSketchElement(std::optional<std::uint64_t> detected, QPoint framebuffer,
                                       double ratio) {
    glyphOverlay->setHoveredPoint(std::nullopt);
    if (detected)
        return detected;
    const auto nearby = sketchPointNear(framebuffer, ratio);
    return nearby ? std::optional{nearby->first.elementId} : std::nullopt;
}

// The click stays where it was (trimming asks which side of it goes); the point it means is
// judged in pixels, as the pointer's position is rounded to the grid. At a shared corner the
// element under the pointer keeps the pick.
void CadViewportScene::notifySketchPick(std::uint64_t element, QPoint framebuffer, bool additive,
                                        double ratio) {
    const auto local = sketchEditor.projectPoint(framebuffer.x(), framebuffer.y());
    if (!sketchElementSelectionHandler || !local)
        return;
    SketchSelectionIntent intent{element, local->first, local->second, additive};
    if (const auto nearby = sketchPointNear(framebuffer, ratio)) {
        const SceneSketchPoint& point = nearby->first;
        const auto own = std::ranges::find_if(pendingSketchElements, [&](const auto& candidate) {
            return candidate.id == element &&
                   std::ranges::any_of(candidate.points, [&](const SceneSketchPoint& other) {
                       return other.kind != SketchPointKind::midpoint &&
                              std::hypot(other.localX - point.localX, other.localY - point.localY) <
                                  1.0e-9;
                   });
        });
        if (own == pendingSketchElements.end())
            intent.elementId = point.elementId;
        intent.point = std::pair{point.localX, point.localY};
    }
    sketchElementSelectionHandler(intent);
}

void CadViewport::mouseMoveEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready) {
            return;
        }

        const QPoint position = framebufferPoint(*this, event->position());
        if (impl_->gesture == Impl::Gesture::selectBox) {
            impl_->selectionBand->setCrossing(position.x() < impl_->gestureOrigin.x());
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
        if (impl_->sketchEditor.isActive() && event->buttons() == Qt::NoButton) {
            impl_->sketchPointer = position; // D01: typed values are previewed here
            impl_->sketchEditor.setSnapBypass(event->modifiers().testFlag(Qt::ControlModifier));
            if (impl_->snapOverlay != nullptr)
                impl_->snapOverlay->display(
                    impl_->sketchEditor.snapFeedback(position.x(), position.y()), event->position(),
                    devicePixelRatioF());
            if (impl_->sketchEditorPreviewHandler) {
                impl_->sketchEditorPreviewHandler(
                    impl_->sketchEditor.move(position.x(), position.y()));
            }
            impl_->showDynamicFields(event->position());
            // User report 2026-10-23: lines light up under the pointer in a sketch too.
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            impl_->glyphOverlay->hover(event->position()); // W18: the symbol names itself
            impl_->sketchPresentation.hover(impl_->context, impl_->glyphOverlay->hoveredElements());
            impl_->hoverSketchPoint(position, devicePixelRatioF()); // VP02
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
        } else if (auto* grip = impl_->activeGrip()) {
            grip->update(position.x(), position.y(), impl_->view,
                         event->modifiers().testFlag(Qt::ControlModifier));
        } else if (impl_->gesture != Impl::Gesture::selectBox) {
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            impl_->sketchRegions.hover(impl_->context, impl_->view, position.x(),
                                       position.y()); // W15
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
        impl_->pressModifiers = event->modifiers(); // N02b
        if (event->button() == Qt::RightButton) {
            impl_->gesture = Impl::Gesture::rotate;
            impl_->view->StartRotation(impl_->gestureOrigin.x(), impl_->gestureOrigin.y());
            setCursor(Qt::ClosedHandCursor);
        } else if (event->button() == Qt::MiddleButton) {
            impl_->gesture = Impl::Gesture::pan;
            impl_->view->Pan(0, 0, 1.0, true);
            setCursor(Qt::SizeAllCursor);
        } else if (event->button() == Qt::LeftButton) {
            const QPoint position = framebufferPoint(*this, event->position());
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            if (impl_->sketchEditor.isActive()) {
                impl_->sketchElementPressed = false;
                // S17: while edges are projected a click takes a body edge only.
                if (impl_->edgePicking) {
                    const auto owner =
                        occ::handle<StdSelect_BRepOwner>::DownCast(impl_->detectedBodyOwner());
                    if (!owner.IsNull() && owner->Shape().ShapeType() == TopAbs_EDGE &&
                        impl_->edgeSelectionHandler)
                        for (const auto& presentation : impl_->presentations)
                            if (presentation.shape == owner->Selectable()) {
                                const auto handler = impl_->edgeSelectionHandler;
                                handler(presentation.id, owner->Shape(), false);
                                break;
                            }
                    update();
                    return;
                }
                if (impl_->sketchEditor.isDrawing())
                    return;
                // S13: an element under the pointer wins over a constraint label on it.
                std::optional<std::uint64_t> element;
                for (impl_->context->InitDetected(); impl_->context->MoreDetected();
                     impl_->context->NextDetected()) {
                    const auto object = occ::handle<AIS_InteractiveObject>::DownCast(
                        impl_->context->DetectedCurrentOwner()->Selectable());
                    if (!element)
                        element = impl_->sketchPresentation.detectedElement(object);
                }
                element = impl_->pressedSketchElement(element, position, devicePixelRatioF());
                const auto constraint = impl_->sketchConstraintPresentation.constraintAt(
                    impl_->view, position.x(), position.y());
                if (!element && constraint && *constraint != 0) {
                    if (impl_->sketchConstraintSelectionHandler)
                        impl_->sketchConstraintSelectionHandler(*constraint);
                    update();
                    return;
                }
                if (element) {
                    const bool additive = event->modifiers().testFlag(Qt::ControlModifier);
                    impl_->sketchElementPressed = !additive;
                    impl_->notifySketchPick(*element, position, additive, devicePixelRatioF());
                    // S14: after the selection, whose refresh resets the tool state; started
                    // before it, the drag point was lost and the element did not follow.
                    if (!additive)
                        static_cast<void>(
                            impl_->sketchEditor.beginElementDrag(position.x(), position.y()));
                    update();
                } else {
                    // S13: the box starts anywhere without an element or label, also over a
                    // body face or an axis.
                    impl_->gesture = Impl::Gesture::selectBox;
                    impl_->selectionBand->setCrossing(false);
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
            if (impl_->startGrip(detected, position.x(), position.y())) {
                setCursor(Qt::ClosedHandCursor);
                return;
            }
            if (impl_->manipulator.matches(detected) && impl_->manipulator.hasActiveMode()) {
                impl_->gesture = Impl::Gesture::moveAxis;
                impl_->manipulator.start(position.x(), position.y(), impl_->view);
                setCursor(Qt::ClosedHandCursor);
                return;
            }
            if (const auto elementId = impl_->detectedSketchElement()) {
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
        impl_->clickCycle = {}; // W07: a double click is no second click at the same place
        // W07: the press just took an edge; the double click takes its tangent chain.
        if (const auto& [body, edge] = impl_->lastPickedEdge; !impl_->sketchEditor.isActive() &&
                                                              body != 0 && !edge.IsNull() &&
                                                              impl_->edgeChainHandler) {
            impl_->edgeChainHandler(body, edge);
            impl_->swallowRelease = true;
            update();
            return;
        }
        const QPoint position = framebufferPoint(*this, event->position());
        // S13: a label anywhere under the pointer, also over its element.
        const auto id = impl_->sketchConstraintPresentation.constraintAt(impl_->view, position.x(),
                                                                         position.y());
        if (id.has_value() && *id != 0 && impl_->sketchConstraintEditHandler)
            impl_->sketchConstraintEditHandler(*id);
    });
}

void CadViewport::mouseReleaseEvent(QMouseEvent* event) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (impl_->ready && event->button() == Qt::LeftButton && impl_->activeGrip() != nullptr) {
            impl_->activeGrip()->finish();
            impl_->gesture = Impl::Gesture::none;
            unsetCursor();
            update();
            return;
        }
        if (impl_->gesture == Impl::Gesture::cancelled) {
            impl_->gesture = Impl::Gesture::none;
            return;
        }
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
            if (impl_->sketchEditor.isActive()) {
                // S17: while edges are projected the press took the edge; the release draws
                // nothing (it would start a line of the active tool).
                if (impl_->edgePicking) {
                    update();
                    return;
                }
                if (impl_->gesture == Impl::Gesture::selectBox) {
                    // S13 (user decision 2026-10-09): left to right a window, right to left a
                    // crossing box; whole elements; Ctrl adds; an empty box (or a click on
                    // nothing) without Ctrl clears the selection.
                    const QRect rectangle(impl_->gestureOrigin, position);
                    const bool crossing = position.x() < impl_->gestureOrigin.x();
                    const bool additive = event->modifiers().testFlag(Qt::ControlModifier);
                    impl_->gesture = Impl::Gesture::none;
                    impl_->selectionBand->hide();
                    const auto ids = boxSelection(impl_->pendingSketchElements, impl_->view,
                                                  rectangle.normalized(), crossing);
                    if (!ids.empty() && impl_->sketchBoxSelectionHandler)
                        impl_->sketchBoxSelectionHandler(ids, additive);
                    else if (ids.empty() && !additive && impl_->emptySelectionHandler)
                        impl_->emptySelectionHandler();
                    update();
                    return;
                }
                if (impl_->sketchElementPressed) {
                    impl_->sketchElementPressed = false;
                    impl_->sketchEditor.finishElementDrag();
                    if (impl_->sketchEditorDragEndHandler)
                        impl_->sketchEditorDragEndHandler();
                    return;
                }
                const auto intent = impl_->sketchEditor.isDrawing()
                                        ? impl_->sketchEditor.click(position.x(), position.y())
                                        : std::nullopt;
                if (intent.has_value() && impl_->sketchEditorElementHandler) {
                    impl_->sketchEditorElementHandler(*intent);
                }
                if (impl_->sketchEditorPreviewHandler)
                    impl_->sketchEditorPreviewHandler(std::nullopt);
                impl_->showDynamicFields(event->position()); // W13: none once the element is placed
                update();
                return;
            }
            // The area of a sketch region wins over the base plane behind it and over empty
            // space (user report 2026-10-05).
            const auto clickRay = [&] {
                double x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, ex = 0, ey = 0, ez = 0;
                impl_->view->ConvertWithProj(position.x(), position.y(), x, y, z, vx, vy, vz);
                impl_->view->Proj(ex, ey, ez);
                return ViewRay{
                    {x, y, z}, {-ex, -ey, -ez}, event->modifiers().testFlag(Qt::ControlModifier)};
            };
            const auto areaClick = [&](std::optional<gp_Pnt> surface = std::nullopt) {
                if (!impl_->areaClickHandler || !impl_->selectionFilter.sketches) // W07
                    return false;
                ViewRay ray = clickRay();
                if (surface)
                    ray.nearestSurface =
                        gp_Vec(gp_Pnt(ray.origin[0], ray.origin[1], ray.origin[2]), *surface)
                            .Dot(gp_Vec(ray.direction[0], ray.direction[1], ray.direction[2]));
                if (!impl_->areaClickHandler(ray))
                    return false;
                impl_->context->ClearSelected(false);
                update();
                return true;
            };
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            // M06: an open fillet takes edges only; a click elsewhere changes nothing.
            if (impl_->edgePicking) {
                // A chosen edge drawn over the preview wins and is reported with body 0.
                occ::handle<StdSelect_BRepOwner> chosen;
                for (impl_->context->InitDetected(); impl_->context->MoreDetected();
                     impl_->context->NextDetected()) {
                    const auto candidate = occ::handle<StdSelect_BRepOwner>::DownCast(
                        impl_->context->DetectedCurrentOwner());
                    if (!candidate.IsNull() && !impl_->edgeHighlights[0].IsNull() &&
                        candidate->Selectable() == impl_->edgeHighlights[0]) {
                        chosen = candidate;
                        break;
                    }
                }
                if (!chosen.IsNull() && impl_->edgeSelectionHandler) {
                    const auto handler = impl_->edgeSelectionHandler;
                    handler(0, chosen->Shape(), false);
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                const auto owner =
                    occ::handle<StdSelect_BRepOwner>::DownCast(impl_->detectedBodyOwner());
                if (!owner.IsNull() && owner->Shape().ShapeType() == TopAbs_EDGE &&
                    impl_->edgeSelectionHandler) {
                    const auto object =
                        occ::handle<AIS_InteractiveObject>::DownCast(owner->Selectable());
                    for (const auto& presentation : impl_->presentations)
                        if (presentation.shape == object) {
                            const auto handler = impl_->edgeSelectionHandler;
                            handler(presentation.id, owner->Shape(), false);
                            break;
                        }
                }
                impl_->gesture = Impl::Gesture::none;
                unsetCursor();
                update();
                return;
            }
            if (impl_->context->HasDetected()) {
                const occ::handle<AIS_InteractiveObject> detected =
                    impl_->context->DetectedInteractive();
                if (impl_->detectedSketchElement().has_value()) {
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                // M19: a construction plane or axis; a body behind a plane wins, as for the
                // base planes.
                if (const auto datum = impl_->datums.datum(detected)) {
                    const auto bodyOwner = impl_->detectedBodyOwner();
                    if (!bodyOwner.IsNull())
                        impl_->notifyBodyOwner(bodyOwner, true);
                    else if (impl_->datumSelectionHandler)
                        impl_->datumSelectionHandler(*datum);
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                if (cycleClick(*event, position) ||
                    impl_->pickBodyEdge(impl_->context->DetectedOwner(),
                                        event->modifiers().testFlag(Qt::ControlModifier))) {
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                // U06: a body behind a translucent base plane wins the click.
                if (impl_->basePlanes.plane(detected).has_value()) {
                    const auto bodyOwner = impl_->detectedBodyOwner();
                    if (!bodyOwner.IsNull()) {
                        impl_->notifyBodyOwner(bodyOwner, true);
                        impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
                        const auto current = impl_->detectedBodyOwner();
                        if (!current.IsNull())
                            impl_->context->SetSelected(current, false);
                        impl_->gesture = Impl::Gesture::none;
                        unsetCursor();
                        update();
                        return;
                    }
                }
                if (impl_->basePlanes.plane(detected).has_value() && areaClick()) {
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    return;
                }
                if (const auto plane = impl_->basePlanes.plane(detected)) {
                    impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
                    if (impl_->basePlaneSelectionHandler)
                        impl_->basePlaneSelectionHandler(*plane);
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    update();
                    return;
                }
                // S14: a sketch region on the clicked body face (a face sketch) wins over the
                // face, as on a base plane; regions behind the face stay hidden.
                if (!impl_->detectedBodyOwner().IsNull() &&
                    impl_->context->MainSelector()->NbPicked() > 0 &&
                    areaClick(impl_->context->MainSelector()->PickedPoint(1))) {
                    impl_->clickCycle = {}; // a region took the click; no cycle from here
                    impl_->gesture = Impl::Gesture::none;
                    unsetCursor();
                    return;
                }
                impl_->notifyDetectedBody(true);
            }
            // A click selects only the topmost face. SelectPoint would select every owner the
            // pick ray crosses (e.g. a cylinder's cap and its far side), and the callback above
            // may have rebuilt the presentations, so detect again before selecting.
            impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
            if (impl_->context->HasDetected()) {
                impl_->context->SelectDetected(AIS_SelectionScheme_Replace);
            } else if (!areaClick()) {
                impl_->context->ClearSelected(false);
                if (impl_->emptySelectionHandler)
                    impl_->emptySelectionHandler();
            }
            update();
        }

        impl_->gesture = Impl::Gesture::none;
        unsetCursor();
    });
}

// S14 (A-TXN-03): Escape during a body drag ends it without a move: no document change and no
// undo step. Other keys (and Escape otherwise) go on to the window's shortcuts.
void CadViewport::keyPressEvent(QKeyEvent* event) {
    if (handleDynamicKey(*event)) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && impl_->gesture == Impl::Gesture::moveAxis) {
        performViewportOperation(*this, impl_->ready, [&] {
            impl_->manipulator.cancel();
            impl_->gesture = Impl::Gesture::cancelled;
            unsetCursor();
            update();
        });
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
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
