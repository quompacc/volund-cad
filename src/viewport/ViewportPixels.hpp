#pragma once
#include <QPointF>
#include <QRect>
#include <QWidget>

namespace voelund::viewport {
// Qt input and child widgets use logical pixels. OCCT Convert/MoveTo and
// gestures use framebuffer pixels. Convert exactly once at that boundary.
inline QPoint framebufferPoint(const QWidget& widget, QPointF logical) {
    return (logical * widget.devicePixelRatioF()).toPoint();
}
inline QPoint logicalPoint(const QWidget& widget, QPoint physical) {
    return (QPointF(physical) / widget.devicePixelRatioF()).toPoint();
}
inline QRect logicalRectangle(const QWidget& widget, QPoint first, QPoint second) {
    return QRect(logicalPoint(widget, first), logicalPoint(widget, second)).normalized();
}
} // namespace voelund::viewport
