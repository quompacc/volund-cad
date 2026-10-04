#include "ViewportLifecycleSupport.hpp"
#include <QApplication>
#include <QDir>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// Regression for a face pick in the real main window (reported 2026-10-04): clicking the top
// face must select and highlight only that face, not the whole body.
namespace {
using namespace voelund;
using test::require;
void facePick() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const auto click = [&](double x, double y, double z, const char* name) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            viewport->grabFramebuffer().save(directory + "/face-pick-" + name + ".png");
        }
        const auto type = viewport::ViewportTestAccess::selectedShapeType(*viewport);
        std::cout << name << " selected owner type: " << (type ? static_cast<int>(*type) : -1)
                  << '\n';
        require(type == TopAbs_FACE && viewport::ViewportTestAccess::selectedCount(*viewport) == 1,
                "Face pick must select exactly the topmost face");
    };
    click(4, 4, 20, "box-top");
    click(20, 15, 45, "cylinder-top"); // Default cylinder: r 8, h 25 on the box top centre.
    std::cout << "PASS main-window face pick selects only the face\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        facePick();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
