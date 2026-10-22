// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ViewportLifecycleSupport.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QWindow>
#include <iostream>
#include <psapi.h>
#include <windows.h>

namespace {
using namespace voelund::test;
using namespace voelund::viewport;

QJsonObject memorySample() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    DWORD handles = 0;
    require(GetProcessMemoryInfo(GetCurrentProcess(),
                                 reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                 sizeof(counters)),
            "Cannot sample process memory");
    require(GetProcessHandleCount(GetCurrentProcess(), &handles), "Cannot sample handles");
    return {{"privateBytes", static_cast<qint64>(counters.PrivateUsage)},
            {"workingSetBytes", static_cast<qint64>(counters.WorkingSetSize)},
            {"handles", static_cast<int>(handles)}};
}

void pick(CadViewport& v) {
    std::uint64_t selected = 0;
    v.setBodySelectionHandler([&](std::uint64_t id) { selected = id; });
    // Offset small target: unscaled coordinates cannot accidentally hit a large central body.
    const auto point = ViewportTestAccess::project(v, 32, 12, 4);
    require(v.rect().contains(point), "Known point outside viewport");
    mouse(v, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(v, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    v.setBodySelectionHandler({});
    require(selected == 42, "DPI known-point picking missed body 42");
    mouse(v, QEvent::MouseMove, {-10, -10}, Qt::NoButton, Qt::NoButton);
}

QJsonObject checkpoint(CadViewport& v, const QString& name, const QString& directory) {
    render(v);
    pick(v);
    render(v);
    const QImage image = v.grabFramebuffer();
    const QSize expected = (QSizeF(v.size()) * v.devicePixelRatioF()).toSize();
    require(!image.isNull() && image.size() == expected, "Qt framebuffer size/DPR mismatch");
    if (ViewportTestAccess::windowSize(v) != image.size())
        std::cerr << name.toStdString() << ": OCCT " << ViewportTestAccess::windowSize(v).width()
                  << "x" << ViewportTestAccess::windowSize(v).height() << " image " << image.width()
                  << "x" << image.height() << std::endl;
    require(ViewportTestAccess::windowSize(v) == image.size(), "OCCT window/FBO size mismatch");
    require(std::abs(ViewportTestAccess::camera(v)->Aspect() -
                     static_cast<double>(image.width()) / image.height()) < 1e-6,
            "Camera aspect differs from framebuffer after resize/recreation");
    require(std::abs(ViewportTestAccess::windowRatio(v) - v.devicePixelRatioF()) < 1e-6,
            "OCCT window DPR stale");
    const auto logical = ViewportTestAccess::project(v, 32, 12, 4);
    const QPoint pixel = (QPointF(logical) * v.devicePixelRatioF()).toPoint();
    const QColor body = image.pixelColor(pixel);
    const QColor background = image.pixelColor(image.width() - 5, 5);
    const int contrast = std::abs(body.red() - background.red()) +
                         std::abs(body.green() - background.green()) +
                         std::abs(body.blue() - background.blue());
    if (contrast <= 60)
        std::cerr << name.toStdString() << ": target " << body.name().toStdString()
                  << " background " << background.name().toStdString() << std::endl;
    if (!directory.isEmpty())
        require(image.save(directory + "/" + name + ".png"), "DPI capture failed");
    require(contrast > 60, "Rendered target absent or framebuffer black/stale");
    return {{"case", name},
            {"status", "passed"},
            {"dpr", v.devicePixelRatioF()},
            {"width", image.width()},
            {"height", image.height()},
            {"qtFbo", static_cast<int>(v.defaultFramebufferObject())},
            {"frames", static_cast<qint64>(ViewportTestAccess::frames(v))}};
}

void overlay(CadViewport& v, const QString& directory) {
    v.beginSketchEditor({42, 0, 0, 4, 0, 0, 1, 1, 0, 0}, SketchElementTool::select);
    const QPoint first(8, 8), last(65, 45);
    mouse(v, QEvent::MouseButtonPress, first, Qt::LeftButton, Qt::LeftButton);
    mouse(v, QEvent::MouseMove, last, Qt::NoButton, Qt::LeftButton);
    require(ViewportTestAccess::selectionBand(v) == QRect(first, last),
            "Selection overlay not in logical widget coordinates");
    if (!directory.isEmpty())
        require(v.grab().save(directory + "/selection-overlay.png"), "Overlay capture failed");
    mouse(v, QEvent::MouseButtonRelease, last, Qt::LeftButton, Qt::NoButton);
    v.cancelSketch();
}

void framebufferReplacement(CadViewport& v) {
    v.makeCurrent();
    {
        QOpenGLFramebufferObject alternate(318, 214,
                                           QOpenGLFramebufferObject::CombinedDepthStencil);
        require(alternate.isValid() && alternate.bind(), "Cannot create alternate FBO");
        v.context()->functions()->glViewport(0, 0, 318, 214);
        require(ViewportTestAccess::bindFramebuffer(v, 2.25), "Alternate FBO rejected");
        require(ViewportTestAccess::windowSize(v) == QSize(318, 214) &&
                    ViewportTestAccess::windowRatio(v) == 2.25,
                "FBO/DPR replacement not propagated");
        // Ratio-only change, with identical pixel dimensions, must also propagate.
        require(alternate.bind(), "Cannot rebind alternate FBO");
        v.context()->functions()->glViewport(0, 0, 318, 214);
        require(ViewportTestAccess::bindFramebuffer(v, 1.25) &&
                    ViewportTestAccess::windowRatio(v) == 1.25,
                "Ratio-only change lost");
        v.makeCurrent();
        require(ViewportTestAccess::bindFramebuffer(v, v.devicePixelRatioF()),
                "Cannot restore widget FBO");
    }
    v.doneCurrent();
    render(v);
}

void foreignContext(CadViewport& v) {
    CadViewport other;
    other.resize(320, 240);
    other.show();
    render(other);
    other.makeCurrent();
    v.fitView();
    require(QOpenGLContext::currentContext() == other.context(),
            "Viewport operation did not restore foreign context");
    other.doneCurrent();
}

QJsonArray stress() {
    QJsonArray samples;
    for (int cycle = 0; cycle < 50; ++cycle) {
        {
            CadViewport v;
            v.resize(400, 300);
            v.setShape(BRepPrimAPI_MakeBox(10, 10, 10).Shape());
            v.show();
            render(v);
            v.setCameraView(CameraView::top);
            v.fitView();
            v.resize(430, 330);
            render(v);
        }
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        auto sample = memorySample();
        sample["cycle"] = cycle + 1;
        samples.append(sample);
    }
    return samples;
}

QJsonArray screens() {
    QJsonArray result;
    for (const auto* screen : QApplication::screens()) {
        result.append(QJsonObject{{"name", screen->name()},
                                  {"dpr", screen->devicePixelRatio()},
                                  {"logicalDpi", screen->logicalDotsPerInch()},
                                  {"width", screen->geometry().width()},
                                  {"height", screen->geometry().height()}});
    }
    return result;
}
QJsonObject scenarios(const QString& directory) {
    QJsonArray cases;
    QMainWindow first, second;
    auto* v = new CadViewport;
    first.setCentralWidget(v);
    first.resize(640, 480);
    const SceneBody bodies[] = {
        {41, BRepPrimAPI_MakeBox(4, 4, 4).Shape(), true},
        {42, BRepPrimAPI_MakeBox(gp_Pnt(30, 10, 0), 4, 4, 4).Shape(), true}};
    v->setBodies(bodies);
    first.show();
    render(*v);
    v->setCameraView(CameraView::top);
    cases.append(checkpoint(*v, "initial", directory));
    overlay(*v, directory);
    framebufferReplacement(*v);
    foreignContext(*v);
    cases.append(checkpoint(*v, "after-controlled-fbo-switch", directory));
    first.resize(713, 517);
    cases.append(checkpoint(*v, "resized", directory));
    first.showMinimized();
    QApplication::processEvents();
    require(first.isMinimized(), "Minimize state not reached");
    first.showNormal();
    cases.append(checkpoint(*v, "restored", directory));
    first.showMaximized();
    QApplication::processEvents();
    require(first.isMaximized(), "Maximize state not reached");
    cases.append(checkpoint(*v, "maximized", directory));
    first.showNormal();
    const QPointer<QOpenGLContext> oldContext = v->context();
    const auto generation = v->lifecycleDiagnostics().contextGeneration;
    first.takeCentralWidget();
    second.resize(640, 480);
    v->setParent(&second);
    second.setCentralWidget(v);
    second.show();
    cases.append(checkpoint(*v, "recreated", directory));
    require(oldContext.isNull() && v->lifecycleDiagnostics().contextGeneration > generation,
            "DPI test did not recreate context");
    QString screenStatus = "blocked-external: only one screen exposed by Qt";
    if (QApplication::screens().size() > 1) {
        for (auto* screen : QApplication::screens()) {
            second.windowHandle()->setScreen(screen);
            second.move(screen->availableGeometry().topLeft() + QPoint(20, 20));
            render(*v);
            require(second.windowHandle()->screen() == screen, "Screen switch failed");
            cases.append(checkpoint(*v, "screen-" + QString::number(cases.size()), directory));
        }
        screenStatus = "passed: actual Qt screen switches; physical display not observed";
    }
    second.hide();
    return {
        {"cases", cases},
        {"screenSwitch", screenStatus},
        {"controlledFboAndRatioSwitch", "passed: injected adapter exercise, not monitor change"},
        {"foreignContextRestoration", "passed"}};
}
} // namespace

void runDpiTests() {
    const QString factor = qEnvironmentVariable("QT_SCALE_FACTOR");
    const QString root = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    const QString directory = root.isEmpty() ? QString{} : root + "/dpi-" + factor;
    if (!directory.isEmpty())
        require(QDir().mkpath(directory), "Cannot create DPI artifact directory");
    auto report = scenarios(directory);
    const auto samples = stress();
    const QJsonObject environment{
        {"schemaVersion", 1},
        {"qpa", QGuiApplication::platformName()},
        {"qt", qVersion()},
        {"scaleFactor", factor},
        {"scaleMethod", "QT_SCALE_FACTOR test override; native screen DPR multiplies it"},
        {"screens", screens()},
        {"resourceSamples", samples},
        {"resourceAssessment", "measurement only; no universal memory leak threshold"}};
    for (auto it = environment.begin(); it != environment.end(); ++it)
        report[it.key()] = it.value();
    if (!directory.isEmpty()) {
        QFile file(directory + "/diagnostics.json");
        require(file.open(QIODevice::WriteOnly), "Cannot write DPI report");
        require(file.write(QJsonDocument(report).toJson()) > 0, "DPI report write failed");
    }
    std::cout << "PASS DPI " << factor.toStdString()
              << ": picking, physical FBO/window, overlay, resize, minimize/maximize, recreate; 50 "
                 "cycles\n"
              << QJsonDocument(report).toJson(QJsonDocument::Compact).toStdString() << '\n';
}
