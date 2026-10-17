#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S20 (user decision 2026-10-14): "Skalieren" and "Drehen" in the sketch panel, one after the
// other on the same rectangle. The four sides picked with Ctrl-click; the center starts at the
// middle of the selection (10, 5); factor 2 makes it 40 x 20, then 90 degrees turns it upright
// about the same center; OK stores x 0..20, y -15..25.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

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
void click(viewport::CadViewport& viewport, double x, double y,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(
            type, QPointF(point), QPointF(viewport.mapToGlobal(point)), Qt::LeftButton,
            type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, modifiers);
        QApplication::sendEvent(&viewport, &event);
    }
    test::render(viewport);
    settle();
}
QDoubleSpinBox& field(QWidget& window, const char* id) {
    auto* input =
        window.findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(id));
    require(input != nullptr, std::string("Field ") + id);
    return *input;
}
// The four sides by their middles, the first without Ctrl.
void pickSides(QWidget& window, viewport::CadViewport& viewport,
               std::initializer_list<std::pair<double, double>> middles) {
    command(window, "sketch.tool.select").trigger();
    settle();
    bool first = true;
    for (const auto& [x, y] : middles) {
        click(viewport, x, y, first ? Qt::NoModifier : Qt::ControlModifier);
        first = false;
    }
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);

    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, 0, 0);
    click(*viewport, 20, 10);

    pickSides(window, *viewport, {{10, 0}, {20, 5}, {10, 10}, {0, 5}});
    require(command(window, "sketch.scale").isEnabled() &&
                std::abs(field(window, "transform_center_x").value() - 10) < 1e-6 &&
                std::abs(field(window, "transform_center_y").value() - 5) < 1e-6,
            "Skalieren offered; the center is the middle of the selection");
    field(window, "scale_factor").setValue(2);
    settle();
    command(window, "sketch.scale").trigger();
    settle();
    test::render(*viewport);

    pickSides(window, *viewport, {{10, -5}, {30, 5}, {10, 15}, {-10, 5}});
    field(window, "rotate_angle").setValue(90);
    settle();
    command(window, "sketch.rotate").trigger();
    settle();
    test::render(*viewport);

    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    double left = 1e9, right = -1e9, bottom = 1e9, top = -1e9;
    for (const auto& element :
         session.sketchParameters(session.features().back().id).elements.values)
        if (const auto* line = std::get_if<document::SketchLine>(&element.geometry))
            for (const auto& point : {line->start, line->end}) {
                left = std::min(left, point.x);
                right = std::max(right, point.x);
                bottom = std::min(bottom, point.y);
                top = std::max(top, point.y);
            }
    require(std::abs(left) < 1e-6 && std::abs(right - 20) < 1e-6 && std::abs(bottom + 15) < 1e-6 &&
                std::abs(top - 25) < 1e-6,
            "Scaled by 2, then turned upright: x 0..20, y -15..25, got x " + std::to_string(left) +
                ".." + std::to_string(right) + ", y " + std::to_string(bottom) + ".." +
                std::to_string(top));
    std::cout << "PASS S20 sketch rotate/scale flow: scale 2, rotate 90, OK\n";
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
