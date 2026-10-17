#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// User reports 2026-10-05: construction geometry is drawn dashed; after leaving the sketch a
// click into the area of a rectangle picks it for the extrusion (not only a click on its edge).
namespace {
using namespace voelund;
using test::require;
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
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
int dashedCount(viewport::CadViewport& viewport) {
    int count = 0;
    for (std::uint64_t id = 1; id <= 8; ++id)
        if (viewport::ViewportTestAccess::sketchElementDashed(viewport, id).value_or(false))
            ++count;
    return count;
}

// Free geometry is blue, determined geometry dark (light theme).
std::pair<int, int> freeAndDetermined(viewport::CadViewport& viewport) {
    int free = 0, determined = 0;
    for (std::uint64_t id = 1; id <= 8; ++id)
        if (const auto color = viewport::ViewportTestAccess::sketchElementColor(viewport, id)) {
            if (color->Blue() - color->Red() > 0.3)
                ++free;
            else if (color->Red() < 0.2 && color->Green() < 0.2 && color->Blue() < 0.2)
                ++determined;
        }
    return {free, determined};
}
QString status(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* label = panel->findChild<QLabel*>(QStringLiteral("toolStatus"));
    return label != nullptr && label->isVisible() ? label->text() : QString();
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
    click(*viewport, -20, -10);
    click(*viewport, 20, 10);
    require(dashedCount(*viewport) == 0, "Regular geometry is solid");
    command(window, "sketch.tool.select").trigger();
    click(*viewport, 300, 300); // The last drawn side is selected; colors show selection.
    require(status(window).startsWith(QStringLiteral("Freiheitsgrade: ")) &&
                freeAndDetermined(*viewport) == std::pair{4, 0},
            "A new rectangle is free: status line and blue sides, got " +
                status(window).toStdString());

    // Fixing the top side determines it, and only it (user decision 2026-10-05).
    command(window, "sketch.tool.select").trigger();
    click(*viewport, 0, 10);
    command(window, "sketch.fixed").trigger();
    settle();
    test::render(*viewport);
    command(window, "sketch.tool.select").trigger();
    click(*viewport, 300, 300); // Nothing selected, so colors show the state.
    test::render(*viewport);
    const auto [free, determined] = freeAndDetermined(*viewport);
    require(determined == 1 && free == 3, "Fixed side dark, the others still blue (" +
                                              std::to_string(free) + " free, " +
                                              std::to_string(determined) + " determined)");
    command(window, "sketch.undo_sketch").trigger();
    settle();
    test::render(*viewport);

    // One side becomes construction geometry: it, and only it, is dashed.
    command(window, "sketch.tool.select").trigger();
    click(*viewport, 0, 10);
    QAction& construction = command(window, "sketch.construction");
    require(construction.isEnabled(), "A side is selected");
    construction.trigger();
    settle();
    test::render(*viewport);
    require(dashedCount(*viewport) == 1, "Construction geometry is dashed");
    command(window, "sketch.undo_sketch").trigger();
    settle();
    test::render(*viewport);
    require(dashedCount(*viewport) == 0, "Undo makes it regular again");

    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* accept = panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(accept != nullptr && accept->isEnabled(), "The rectangle can be accepted");
    accept->click();
    settle();
    test::render(*viewport);
    require(session.features().size() == 1, "Sketch created");

    // A click into the area, away from the edges, picks the region.
    QAction& extrude = command(window, "solid.create.extrude_profiles");
    require(!extrude.isEnabled(), "Nothing to extrude yet");
    click(*viewport, 6, 3);
    require(extrude.isEnabled() && session.selectedSketchProfiles().size() == 1,
            "A click into the rectangle picks its area");
    click(*viewport, 200, 200);
    require(!extrude.isEnabled(), "A click outside deselects it");
    std::cout << "PASS sketch feedback: dashed construction, area pick\n";
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
