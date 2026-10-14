#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S17 (user decision 2026-10-14): in a sketch on XY beside a box, "Projizieren" turns edge
// picking on; two box edges clicked in a row become two construction lines, a third click on
// the first edge (now under its projection) adds nothing; "Projizieren" again ends it, and OK
// stores both projections.
namespace {
using namespace voelund;
using test::require;

QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    settle();
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

void run() {
    application::CadSession session;
    session.newDocument();
    const auto box = session.createBoxFeature({20, 20, 10});
    require(box.feature().has_value(), "Box");
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    window.refreshAfterExternalChange();
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);

    Bnd_Box bounds;
    BRepBndLib::Add(session.validBodies().front().shape, bounds);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    bounds.Get(x0, y0, z0, x1, y1, z1);

    // The XY base plane beside the box carries the sketch.
    click(*viewport, x1 + 15, y1 + 15, 0);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    QAction& project = command(window, "sketch.project");
    require(project.isEnabled(), "Projizieren is offered in the sketch");
    project.trigger();
    settle();

    const double xMiddle = (x0 + x1) / 2, yMiddle = (y0 + y1) / 2;
    click(*viewport, xMiddle, y0, z1);
    require(viewport->displayedSketchElementCount() == 1, "The first edge is projected");
    click(*viewport, x1, yMiddle, z1);
    require(viewport->displayedSketchElementCount() == 2, "A second edge right after");
    click(*viewport, xMiddle, y0, z1);
    require(viewport->displayedSketchElementCount() == 2,
            "The first edge again, under its projection, adds nothing");

    project.trigger();
    settle();
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    const auto& sketch = session.sketchParameters(session.features().back().id);
    require(session.features().back().kind == document::FeatureKind::sketch &&
                sketch.projections.size() == 2 && sketch.elements.values.size() == 2,
            "OK stores two projections");
    for (const auto& element : sketch.elements.values) {
        const auto& line = std::get<document::SketchLine>(element.geometry);
        require(element.construction &&
                    std::abs(std::hypot(line.end.x - line.start.x, line.end.y - line.start.y) -
                             20) < 1e-6,
                "Each is a construction line of the edge length");
    }
    std::cout << "PASS S17 sketch projection flow: two edges, repeat refused, OK\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
