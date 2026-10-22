// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#if !defined(VOELUND_VIEWPORT_QT_OPENGL)
// Include GDI before OCCT's lean Windows declarations suppress this API surface.
#include <Windows.h>
#include <gl/GL.h>
#endif

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QSysInfo>
#include <QTimer>
#include <QWheelEvent>
#include <Standard_Version.hxx>

#include <iostream>

namespace {

using voelund::viewport::CadViewport;

#if !defined(VOELUND_VIEWPORT_QT_OPENGL)
bool captureNativeFramebuffer(CadViewport& viewport, const QString& stem) {
    const auto window = reinterpret_cast<HWND>(viewport.winId());
    if (wglGetCurrentContext() == nullptr || WindowFromDC(wglGetCurrentDC()) != window)
        return false;
    const auto [width, height] = viewport.renderingPixelSize();
    if (width <= 0 || height <= 0)
        return false;
    QImage image(width, height, QImage::Format_RGBA8888);
    GLint previousBuffer = 0;
    GLint previousAlignment = 0;
    glGetIntegerv(GL_READ_BUFFER, &previousBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    const bool success = glGetError() == GL_NO_ERROR;
    glReadBuffer(static_cast<GLenum>(previousBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
    return success && image.flipped(Qt::Vertical).save(stem + QStringLiteral("-framebuffer.png"));
}
#endif

bool capture(QWidget& window, CadViewport& viewport, const QString& stem) {
    // QWidget capture is independent of occluding desktop windows. For the native
    // backend it intentionally excludes GL; the framebuffer is saved separately.
    if (!window.grab().save(stem + QStringLiteral("-widgets.png")))
        return false;
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    if (!viewport.grabFramebuffer().save(stem + QStringLiteral("-framebuffer.png")))
        return false;
#else
    if (!captureNativeFramebuffer(viewport, stem))
        return false;
#endif
    const auto [pixelWidth, pixelHeight] = viewport.renderingPixelSize();
    const QJsonObject metadata{
        {QStringLiteral("qt"), QString::fromLatin1(qVersion())},
        {QStringLiteral("occt"), QStringLiteral(OCC_VERSION_COMPLETE)},
        {QStringLiteral("os"), QSysInfo::prettyProductName()},
        {QStringLiteral("qpa"), QGuiApplication::platformName()},
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
        {QStringLiteral("backend"), QStringLiteral("qt-opengl")},
#else
        {QStringLiteral("backend"), QStringLiteral("native")},
#endif
        {QStringLiteral("ready"), viewport.isRenderingReady()},
        {QStringLiteral("windowWidth"), window.width()},
        {QStringLiteral("windowHeight"), window.height()},
        {QStringLiteral("devicePixelRatio"), viewport.devicePixelRatioF()},
        {QStringLiteral("logicalDpi"), window.screen()->logicalDotsPerInch()},
        {QStringLiteral("renderWidth"), pixelWidth},
        {QStringLiteral("renderHeight"), pixelHeight},
        {QStringLiteral("capture"),
         QStringLiteral("QWidget::grab plus separate GL framebuffer; no compositor proof")}};
    QFile file(stem + QStringLiteral(".json"));
    const QByteArray bytes = QJsonDocument(metadata).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool showSlotFixture(CadViewport& viewport) {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const voelund::document::SketchPlane plane{0, 0, 40, 0, 0, 1, 1, 0, 0};
    voelund::application::SketchEditorSession editor(session, support, plane);
    editor.setTool(voelund::application::SketchDrawingTool::slot);
    if (!editor.setPreview({0, 0}, {12, 0}, voelund::document::SketchPoint{0, 3}) ||
        !editor.confirmPreview() || !editor.canAccept())
        return false;
    std::vector<voelund::viewport::SceneSketchElement> elements;
    for (const auto& element : editor.evaluatedElements())
        elements.emplace_back(element.id.value(), element.shape);
    viewport.setTransformToolEnabled(false);
    viewport.setBodies({});
    viewport.setSketchElements(elements);
    viewport.setCameraView(voelund::viewport::CameraView::top);
    return viewport.displayedSketchElementCount() == 4;
}

bool advanceScenario(int stage, QWidget& window, CadViewport& viewport) {
    if (stage == 0) {
        auto* action = window.findChild<QAction*>(QStringLiteral("transformAction"));
        if (action == nullptr || !action->isEnabled())
            return false;
        action->trigger();
    } else if (stage == 1) {
        const QPointF position(viewport.rect().center());
        const QPointF global(viewport.mapToGlobal(position.toPoint()));
        QWheelEvent wheel(position, global, QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&viewport, &wheel);
        if (!wheel.isAccepted())
            return false;
        window.resize(1100, 760);
    } else if (stage == 2) {
        return showSlotFixture(viewport);
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        std::cerr << "Usage: voelund_viewport_visual_review OUTPUT_DIRECTORY\n";
        return 2;
    }
    const QString output = application.arguments().at(1);
    if (!QDir().mkpath(output))
        return 2;
    voelund::application::CadSession session;
    voelund::desktop::MainWindow window(session);
    window.resize(1400, 900);
    window.show();
    auto* viewport = dynamic_cast<CadViewport*>(window.centralWidget());
    if (viewport == nullptr)
        return 3;
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer timer;
    int stage = 0;
    int readyTicks = 0;
    QObject::connect(&timer, &QTimer::timeout, &application, [&] {
        if (elapsed.elapsed() > 15000) {
            application.exit(4);
            return;
        }
        if (!viewport->isRenderingReady() || !window.isVisible()) {
            readyTicks = 0;
            return;
        }
        // Readiness is mandatory. Additional event-loop turns let queued paints
        // and the desktop compositor settle; this is not a pixel-readiness oracle.
        if (++readyTicks < 4)
            return;
        const QString stem = QDir(output).filePath(QString::number(stage));
        if (!capture(window, *viewport, stem) || !advanceScenario(stage, window, *viewport)) {
            application.exit(5);
            return;
        }
        readyTicks = 0;
        if (++stage == 4)
            application.exit(0);
    });
    timer.start(150);
    return application.exec();
}
