#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QDir>
#include <QMainWindow>
#include <QPointer>
#include <iostream>

#ifdef VOELUND_TEST_WINDOWS_DPI
void runDpiTests();
#endif

namespace {
using namespace voelund::test;
using namespace voelund::viewport;

void cycles() {
    for (int cycle = 0; cycle < 10; ++cycle) {
        CadViewport neverShown;
        ViewportTestAccess::cleanup(neverShown);
        const auto cleaned = neverShown.lifecycleDiagnostics().cleanupGeneration;
        ViewportTestAccess::cleanup(neverShown);
        require(neverShown.lifecycleDiagnostics().cleanupGeneration == cleaned,
                "Uninitialized cleanup is not idempotent");
        CadViewport visible;
        visible.setShape(BRepPrimAPI_MakeBox(20, 20, 20).Shape());
        visible.resize(720, 520);
        visible.show();
        render(visible);
    }
    std::cout << "PASS ten unshown/shown destruction cycles\n";
}

void scene(CadViewport& v) {
    const SceneBody body{42, BRepPrimAPI_MakeBox(20, 20, 20).Shape(), true};
    v.setBodies(std::span(&body, 1));
    const SceneSketchElement element{
        7, BRepBuilderAPI_MakeEdge(gp_Pnt(2, 2, 20), gp_Pnt(4, 2, 20)).Shape()};
    v.setSketchElements(std::span(&element, 1), 7);
    const SceneSketchConstraint constraint{9, "P02", 3, 2, 20, false, true};
    v.setSketchConstraints(std::span(&constraint, 1));
}

void pick(CadViewport& v) {
    std::uint64_t selected = 0;
    v.setBodySelectionHandler([&](std::uint64_t id) { selected = id; });
    const QPoint point = ViewportTestAccess::project(v, 12, 12, 20);
    mouse(v, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(v, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    v.setBodySelectionHandler({});
    require(selected == 42, "Known-point body picking failed");
}

// The application refreshes the scene from the selection callback; the click must still
// select the picked face, not the whole body (regression reported on 2026-10-04).
void pickFaceAfterRefresh(CadViewport& v) {
    const SceneBody body{42, BRepPrimAPI_MakeBox(20, 20, 20).Shape(), true};
    std::optional<std::uint64_t> face;
    v.setBodySelectionHandler([&](std::uint64_t) { v.setBodies(std::span(&body, 1)); });
    v.setPlanarFaceSelectionHandler(
        [&](const PlanarFaceSelection& selection) { face = selection.bodyId; });
    const QPoint point = ViewportTestAccess::project(v, 12, 12, 20);
    mouse(v, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(v, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    v.setBodySelectionHandler({});
    v.setPlanarFaceSelectionHandler({});
    require(face == 42, "Face pick did not report the planar face");
    require(ViewportTestAccess::selectedShapeType(v) == TopAbs_FACE,
            "Face pick highlighted the whole body instead of the face");
    std::cout << "PASS face pick after scene refresh selects only the face\n";
}

void failureRecovery(CadViewport& v) {
    const auto camera = ViewportTestAccess::camera(v);
    ViewportTestAccess::cleanup(v);
    const auto clean = v.lifecycleDiagnostics().cleanupGeneration;
    ViewportTestAccess::cleanup(v);
    require(clean == v.lifecycleDiagnostics().cleanupGeneration, "Repeated cleanup not idempotent");
    ViewportTestAccess::initialize(v, true);
    require(!v.isRenderingReady() &&
                ViewportTestAccess::error(v).find("injected") != std::string::npos,
            "Injected partial initialization failure was not reported");
    ViewportTestAccess::initialize(v);
    render(v);
    require(sameCamera(camera, ViewportTestAccess::camera(v)), "Recovery lost camera");
    require(ViewportTestAccess::sceneRestored(v), "Recovery lost scene/selection");
    std::cout << "PASS idempotent cleanup, partial failure and recovery\n";
}

void captureLifecycle(CadViewport& v, const QString& name) {
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    QDir().mkpath(directory);
    require(v.grabFramebuffer().save(directory + "/" + name + ".png"), "Capture failed");
}

void reparentAndInteract() {
    QMainWindow first, second;
    auto* v = new CadViewport;
    first.setCentralWidget(v);
    first.resize(760, 560);
    second.resize(760, 560);
    scene(*v);
    first.show();
    render(*v);
    v->setCameraView(CameraView::top);
    render(*v);
    pick(*v);
    pickFaceAfterRefresh(*v);
    failureRecovery(*v);
    const auto camera = ViewportTestAccess::camera(*v);
    const auto before = v->lifecycleDiagnostics();
    const QPointer<QOpenGLContext> oldContext = v->context();
    first.takeCentralWidget();
    v->setParent(&second);
    second.setCentralWidget(v);
    second.show();
    render(*v);
    const auto after = v->lifecycleDiagnostics();
    require(oldContext.isNull() && after.contextGeneration > before.contextGeneration &&
                after.cleanupGeneration > before.cleanupGeneration,
            "No actual context recreation observed");
    require(sameCamera(camera, ViewportTestAccess::camera(*v)), "Reparent lost camera");
    require(ViewportTestAccess::sceneRestored(*v), "Reparent lost scene/selection");
    pick(*v);
    mouse(*v, QEvent::MouseMove, {-10, -10}, Qt::NoButton, Qt::NoButton);
    render(*v);
    captureLifecycle(*v, "p02-recreated-framebuffer");
    navigation(*v);
    v->setTransformTarget(42);
    v->setTransformToolEnabled(true);
    render(*v);
    const auto handle = ViewportTestAccess::translationHandle(*v);
    require(handle.has_value(), "No translation manipulator handle detected");
    bool moved = false;
    v->setBodyMoveHandler([&](std::uint64_t id, double x, double y, double z) {
        moved = id == 42 && std::abs(x) + std::abs(y) + std::abs(z) > 1e-6;
    });
    // Follow the detected axis; a fixed diagonal can be perpendicular to it or snap to zero.
    const QPoint destination = ViewportTestAccess::translationDestination(*v, *handle);
    drag(*v, Qt::LeftButton, *handle, destination);
    v->setBodyMoveHandler({});
    require(moved, "Manipulator drag did not issue a move intent");
    v->setCameraView(CameraView::isometric);
    mouse(*v, QEvent::MouseMove, {-10, -10}, Qt::NoButton, Qt::NoButton);
    render(*v);
    captureLifecycle(*v, "p02-interacted-framebuffer");
    std::cout
        << "PASS real context recreation, camera, scene, picking, navigation and manipulator\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    app.setQuitOnLastWindowClosed(false);
    try {
#ifdef VOELUND_TEST_WINDOWS_DPI
        if (app.arguments().contains("--dpi")) {
            runDpiTests();
            return 0;
        }
#endif
        cycles();
        reparentAndInteract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
