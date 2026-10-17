#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S10: trim, extend and split through the sketch panel with real clicks. The actions follow
// the selected curve type, a trim names the relations it removed, and a refusal names its
// reason and changes nothing.
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
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void select(QWidget& window, viewport::CadViewport& viewport, double x, double y) {
    command(window, "sketch.tool.select").trigger();
    click(viewport, x, y);
}
QString run(QWidget& window, viewport::CadViewport& viewport, const char* id) {
    command(window, id).trigger();
    settle();
    test::render(viewport);
    return window.findChild<QStatusBar*>()->currentMessage();
}
bool enabled(QWidget& window, const char* id) { return command(window, id).isEnabled(); }
// S11: Ctrl-click adds to the selection.
void addClick(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(
            type, QPointF(point), QPointF(viewport.mapToGlobal(point)), Qt::LeftButton,
            type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::ControlModifier);
        QApplication::sendEvent(&viewport, &event);
    }
    test::render(viewport);
    settle();
}
int redElements(viewport::CadViewport& viewport) {
    int count = 0;
    for (std::uint64_t id = 1; id <= 16; ++id)
        if (const auto color = viewport::ViewportTestAccess::sketchElementColor(viewport, id))
            if (color->Red() > 0.9 && color->Green() < 0.3)
                ++count;
    return count;
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
    command(window, "sketch.tool.circle").trigger();
    click(*viewport, 24, 3); // crosses the right side at y = 0 and y = 6
    click(*viewport, 29, 3);

    select(window, *viewport, 29, 3);
    require(enabled(window, "sketch.trim") && !enabled(window, "sketch.extend") &&
                !enabled(window, "sketch.split"),
            "A circle can only be trimmed");
    select(window, *viewport, 20, 8);
    require(enabled(window, "sketch.trim") && enabled(window, "sketch.extend") &&
                enabled(window, "sketch.split"),
            "A line can be trimmed, extended and split");
    const QString trimmed = run(window, *viewport, "sketch.trim");
    require(trimmed.contains(QStringLiteral("Deckungsgleich")),
            "The trim names the corner relation it removed, got " + trimmed.toStdString());

    select(window, *viewport, 0, 10);
    const QString refused = run(window, *viewport, "sketch.trim");
    require(refused.startsWith(QStringLiteral("Stutzen: ")) &&
                refused.contains(QStringLiteral("keine andere Geometrie")),
            "A side without a cut is refused with its reason, got " + refused.toStdString());

    command(window, "sketch.undo_sketch").trigger();
    settle();
    test::render(*viewport);
    select(window, *viewport, 20, 8);
    const QString split = run(window, *viewport, "sketch.split");
    require(split == QStringLiteral("Skizzenoperation abgeschlossen"),
            "Undo brought the side back; it splits without removing relations, got " +
                split.toStdString());
    // S11: a radius too large names the limit and shows both sides in red; a valid one rounds.
    select(window, *viewport, -10, -10);
    addClick(*viewport, -20, -5);
    auto* radius = window.findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_fillet_radius"));
    require(radius != nullptr && enabled(window, "sketch.fillet"), "Fillet ready for two sides");
    radius->setValue(50);
    const QString tooLarge = run(window, *viewport, "sketch.fillet");
    require(tooLarge.contains(QStringLiteral("zu groß")) && redElements(*viewport) == 2,
            "Too large: reason and both sides red, got " + tooLarge.toStdString() + ", " +
                std::to_string(redElements(*viewport)) + " red");
    radius->setValue(3);
    const QString rounded = run(window, *viewport, "sketch.fillet");
    require(rounded.startsWith(QStringLiteral("Skizzenoperation abgeschlossen")) &&
                redElements(*viewport) == 0,
            "A valid radius rounds the corner, got " + rounded.toStdString());
    // S12: a linear pattern of the top side; a copy cannot be deleted, its pattern dissolves.
    select(window, *viewport, 0, 10);
    const QString patterned = run(window, *viewport, "sketch.linear_pattern");
    require(patterned.contains(QStringLiteral("Muster angelegt")),
            "Pattern created, got " + patterned.toStdString());
    select(window, *viewport, 35, 10);
    require(enabled(window, "sketch.dissolve_pattern") && enabled(window, "sketch.edit_pattern"),
            "A copy names its pattern");
    const QString locked = run(window, *viewport, "sketch.delete");
    require(locked.contains(QStringLiteral("Kopie folgt")),
            "A copy is not deleted, got " + locked.toStdString());
    const QString dissolved = run(window, *viewport, "sketch.dissolve_pattern");
    require(dissolved.contains(QStringLiteral("aufgelöst")) &&
                !enabled(window, "sketch.dissolve_pattern"),
            "Dissolved, got " + dissolved.toStdString());
    std::cout << "PASS sketch curve edit flow\n";
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
