#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QStatusBar>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// V01b: base planes in the viewport of the real main window. Prominent in an empty
// document, pickable, start a sketch on the picked plane, hidden while sketching and faint
// beside a body. Captures are written for the user's visual review.
namespace {
using namespace voelund;
using test::require;
void capture(viewport::CadViewport& viewport, QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    QDir().mkpath(directory);
    viewport.grabFramebuffer().save(directory + "/base-planes-" + name + "-viewport.png");
    window.grab().save(directory + "/base-planes-" + name + "-window.png");
}
void basePlanes() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 3, "Empty document shows three base planes");
    capture(*viewport, window, "empty");

    auto* sketch = window.findChild<QAction*>(QStringLiteral("sketchEditorAction"));
    require(sketch != nullptr && !sketch->isEnabled(), "Sketch needs a selected plane or face");
    const QPoint xy =
        viewport::ViewportTestAccess::project(*viewport, 20, -20, 0); // In front of XZ/YZ.
    test::mouse(*viewport, QEvent::MouseMove, xy, Qt::NoButton, Qt::NoButton);
    test::render(*viewport);
    capture(*viewport, window, "hover-xy");
    test::mouse(*viewport, QEvent::MouseButtonPress, xy, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, xy, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    require(window.statusBar()->currentMessage().startsWith(QStringLiteral("XY")),
            "Click picks the XY plane");
    require(sketch->isEnabled(), "Picking the XY plane enables Sketch");
    capture(*viewport, window, "selected-xy");

    sketch->trigger();
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 0, "Base planes are hidden while sketching");
    capture(*viewport, window, "sketch-on-xy");
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escape);
    QApplication::sendEvent(viewport, &escape);
    test::render(*viewport);

    auto* box = window.findChild<QAction*>(QStringLiteral("addBoxAction"));
    require(box != nullptr && box->isEnabled(), "Add box");
    box->trigger();
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 3, "Base planes stay pickable beside a body");
    viewport->setCameraView(viewport::CameraView::isometric);
    viewport->fitView();
    test::render(*viewport);
    capture(*viewport, window, "with-body");
    std::cout << "PASS base planes: empty, hover, pick, sketch start, hidden, faint beside body\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        basePlanes();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
