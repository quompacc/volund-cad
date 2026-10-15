#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M09 (user decision 2026-10-15): a box 40 x 30 x 20 at the origin. Pick the top face,
// "Spiegeln": the image stands on the top as a new body; "Vereinigen" joins it; the XY plane
// mirrors it below; a click on the front face of the preview makes that face the plane; a
// click on a face of the image itself is refused; the YZ plane from the list; OK as a new body
// is one undo step; the timeline reopens it and joins it; Save/Open.
namespace {
using namespace voelund;
using test::require;

double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies()) {
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        total += properties.Mass();
    }
    return total;
}
bool matches(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& accept(QWidget& window) {
    auto* found = panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(found != nullptr, "OK button");
    return *found;
}
template <class Field> Field* field(QWidget& window, const QString& name) {
    Field* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<Field*>(name))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Panel field");
    return found;
}
QToolButton& timelineStep(QWidget& window, int index) {
    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == index)
            return *step;
    throw std::runtime_error("Timeline step missing");
}
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
double shown(viewport::CadViewport& viewport) {
    test::render(viewport);
    return viewport::ViewportTestAccess::displayedVolume(viewport);
}

void choose(QWidget& window, const char* name, int index) {
    field<QComboBox>(window, QString::fromLatin1(name))->setCurrentIndex(index);
    settle();
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    require(session.createBoxFeature({40, 30, 20}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    const double twice = 2 * 24000;

    click(*viewport, 20, 15, 20);
    QAction& mirror = command(window, "solid.modify.mirror");
    require(mirror.isEnabled(), "Spiegeln offered for a picked face");
    mirror.trigger();
    settle();
    require(panel(window) != nullptr && accept(window).isEnabled() &&
                matches(shown(*viewport), twice),
            "The image on the top face previews as a new body");
    choose(window, "toolChoice_mode", 1);
    require(accept(window).isEnabled() && matches(shown(*viewport), twice), "Joined on the top");
    choose(window, "toolChoice_plane", 0);
    require(accept(window).isEnabled() && matches(shown(*viewport), twice),
            "Joined below the XY plane");

    // Picks over the preview: the original's front face becomes the plane; a face of the
    // image cannot be.
    viewport->setCameraView(viewport::CameraView::front);
    test::render(*viewport);
    click(*viewport, 30, 0, 10);
    require(field<QComboBox>(window, QStringLiteral("toolChoice_plane"))->currentIndex() == 3 &&
                accept(window).isEnabled() && matches(shown(*viewport), twice),
            "A click on the front face of the preview mirrors about it");
    click(*viewport, 30, -30, 10);
    require(field<QComboBox>(window, QStringLiteral("toolChoice_plane"))->currentIndex() == 3 &&
                accept(window).isEnabled() && matches(shown(*viewport), twice),
            "A face of the image is refused; the plane stays");
    choose(window, "toolChoice_plane", 2);
    choose(window, "toolChoice_mode", 0);
    require(accept(window).isEnabled() && matches(shown(*viewport), twice),
            "The YZ plane from the list, as a new body");

    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.features().back().name == "Spiegeln 1" &&
                session.validBodies().size() == 2 && matches(volume(session), twice),
            "OK mirrors the box into a body of its own");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.validBodies().size() == 1 &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();

    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the mirror");
    choose(window, "toolChoice_mode", 1);
    accept(window).click();
    settle();
    require(session.validBodies().size() == 1 && matches(volume(session), twice),
            "Joined into one body");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m09-mirror-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "mirror.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 2 && session.validBodies().size() == 1 &&
                matches(volume(session), twice),
            "Open restores the mirror");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M09 mirror panel: face, join, XY, picks over the preview, image face "
                 "refused, YZ, OK, undo, timeline, open\n";
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
