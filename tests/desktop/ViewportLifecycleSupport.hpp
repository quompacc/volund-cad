#pragma once
#include "ViewportTestAccess.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QThread>
#include <QWheelEvent>
#include <cmath>
#include <stdexcept>

namespace voelund::test {
using viewport::CadViewport;
using viewport::ViewportTestAccess;
inline void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
inline void render(CadViewport& v) {
    const auto before = ViewportTestAccess::frames(v);
    v.update();
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        if (v.isRenderingReady() && ViewportTestAccess::frames(v) > before)
            return;
        QThread::msleep(1);
    }
    throw std::runtime_error("No new ready/rendered frame within 5 seconds");
}
inline void mouse(CadViewport& v, QEvent::Type type, QPoint point, Qt::MouseButton button,
                  Qt::MouseButtons buttons) {
    QMouseEvent event(type, QPointF(point), QPointF(v.mapToGlobal(point)), button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&v, &event);
}
inline void drag(CadViewport& v, Qt::MouseButton button, QPoint from, QPoint to) {
    mouse(v, QEvent::MouseButtonPress, from, button, button);
    mouse(v, QEvent::MouseMove, to, Qt::NoButton, button);
    mouse(v, QEvent::MouseButtonRelease, to, button, Qt::NoButton);
    render(v);
}
inline bool sameCamera(const occ::handle<Graphic3d_Camera>& a,
                       const occ::handle<Graphic3d_Camera>& b) {
    return a->Eye().Distance(b->Eye()) < 1e-7 && a->Center().Distance(b->Center()) < 1e-7 &&
           a->Up().IsEqual(b->Up(), 1e-7) && std::abs(a->Scale() - b->Scale()) < 1e-7 &&
           a->ProjectionType() == b->ProjectionType();
}
inline void navigation(CadViewport& v) {
    auto previous = ViewportTestAccess::camera(v);
    const QPoint center(v.width() / 2, v.height() / 2);
    drag(v, Qt::RightButton, center, center + QPoint(25, 15));
    require(!sameCamera(previous, ViewportTestAccess::camera(v)), "Orbit did not change camera");
    previous = ViewportTestAccess::camera(v);
    drag(v, Qt::MiddleButton, center, center + QPoint(18, 12));
    require(!sameCamera(previous, ViewportTestAccess::camera(v)), "Pan did not change camera");
    previous = ViewportTestAccess::camera(v);
    QWheelEvent wheel(QPointF(center), QPointF(v.mapToGlobal(center)), {}, {0, 120}, Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&v, &wheel);
    render(v);
    require(!sameCamera(previous, ViewportTestAccess::camera(v)), "Wheel did not change camera");
}
} // namespace voelund::test
