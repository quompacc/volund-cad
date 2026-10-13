#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M06 (A-UI-02 for the fillet): "Verrunden" on a selected box opens the panel; a click on an
// edge adds it and the preview shows the rounded body (measured); the radius changes the
// preview; a radius too large keeps the panel open with the reason and OK disabled; Cancel
// keeps the document; OK is one undo step; the timeline reopens the fillet, a click on the
// chosen edge removes it; Save/Open keeps the fillet.
namespace {
using namespace voelund;
using test::require;
constexpr double pi = std::numbers::pi;

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
QString guidance(QWidget& window) {
    auto* label = panel(window)->findChild<QLabel*>(QStringLiteral("toolGuidance"));
    return label != nullptr ? label->text() : QString();
}
QPushButton& button(QWidget& window, const char* name) {
    auto* found = panel(window)->findChild<QPushButton*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}
void setRadius(QWidget& window, double value) {
    QDoubleSpinBox* input = nullptr;
    for (auto* candidate :
         panel(window)->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_radius")))
        if (candidate->isVisible())
            input = candidate;
    require(input != nullptr, "Radius field");
    input->setValue(value);
    settle();
}
QToolButton& timelineStep(QWidget& window, int index) {
    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == index)
            return *step;
    throw std::runtime_error("Timeline step missing");
}

struct Corner final {
    double x, y, z;
};
// The middle of the vertical edge at the box's largest x and y.
Corner corner(const application::CadSession& session) {
    Bnd_Box box;
    BRepBndLib::Add(session.validBodies().front().shape, box);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    box.Get(x0, y0, z0, x1, y1, z1);
    return {x1, y1, (z0 + z1) / 2};
}
void click(viewport::CadViewport& viewport, const Corner& at) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, at.x, at.y, at.z);
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
void openFillet(desktop::MainWindow& window, application::CadSession& session,
                document::FeatureId box) {
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    QAction& fillet = command(window, "solid.modify.fillet");
    require(fillet.isEnabled(), "Verrunden offered for the selected box");
    fillet.trigger();
    settle();
    require(panel(window) != nullptr, "The fillet panel opens");
}

void run() {
    const double one = 10 * (1 - pi / 4);
    const double two = 10 * (4 - pi);
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    const auto created = session.createBoxFeature({20, 20, 10});
    require(created.feature().has_value(), "Box");
    const auto box = *created.feature();
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    const auto edge = corner(session);

    openFillet(window, session, box);
    require(!button(window, "toolAcceptButton").isEnabled() &&
                guidance(window).contains(QStringLiteral("Klicke die Kanten")),
            "Without an edge OK waits");
    click(*viewport, edge);
    require(guidance(window).startsWith(QStringLiteral("1 Kante")) &&
                button(window, "toolAcceptButton").isEnabled() &&
                matches(shown(*viewport), 4000 - one),
            "A clicked edge is previewed with radius 1");
    setRadius(window, 2);
    require(matches(shown(*viewport), 4000 - two), "Radius 2 changes the preview");
    setRadius(window, 25);
    require(panel(window) != nullptr && !button(window, "toolAcceptButton").isEnabled() &&
                guidance(window).contains(QStringLiteral("lassen sich mit dem Radius nicht")),
            "A radius too large keeps the panel open, explained, OK disabled");
    setRadius(window, 1);
    button(window, "toolCancelButton").click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 1 &&
                matches(volume(session), 4000),
            "Cancel keeps the box");

    // User report 2026-10-12: further edges are picked on the previewed body; each one adds,
    // a click on a chosen one removes it again.
    openFillet(window, session, box);
    const Corner second{edge.x, edge.y - 20, edge.z};
    const Corner third{edge.x - 20, edge.y - 20, edge.z};
    click(*viewport, edge);
    click(*viewport, second);
    click(*viewport, third);
    require(guidance(window).startsWith(QStringLiteral("3 Kante")) &&
                matches(shown(*viewport), 4000 - 3 * one),
            "Three edges picked one after the other over the preview");
    click(*viewport, second);
    require(guidance(window).startsWith(QStringLiteral("2 Kante")) &&
                matches(shown(*viewport), 4000 - 2 * one),
            "A click on a chosen edge removes it");
    button(window, "toolCancelButton").click();
    settle();
    require(matches(volume(session), 4000), "Cancel keeps the box");

    openFillet(window, session, box);
    click(*viewport, edge);
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 2 &&
                session.features().back().kind == document::FeatureKind::fillet &&
                matches(volume(session), 4000 - one),
            "OK rounds the edge as a fillet feature");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.features().size() == 1 &&
                session.redo().status() == application::CommandStatus::committed &&
                matches(volume(session), 4000 - one),
            "One undo step");
    window.refreshAfterExternalChange();

    // The timeline reopens the fillet; the radius changes as one undo step.
    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr && guidance(window).startsWith(QStringLiteral("1 Kante")),
            "The timeline step reopens the fillet with its edge");
    setRadius(window, 2);
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2 && matches(volume(session), 4000 - two),
            "OK changes the radius of the same fillet");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 4000 - one),
            "Undo returns to radius 1");
    window.refreshAfterExternalChange();

    // A click on the chosen edge removes it.
    timelineStep(window, 1).click();
    settle();
    click(*viewport, edge);
    require(guidance(window).contains(QStringLiteral("Klicke die Kanten")) &&
                !button(window, "toolAcceptButton").isEnabled(),
            "A click on the chosen edge removes it");
    button(window, "toolCancelButton").click();
    settle();
    require(matches(volume(session), 4000 - one), "Cancel keeps the fillet");

    // Save and Open.
    const auto directory = std::filesystem::temp_directory_path() / "voelund-m06-fillet";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "fillet.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 2 && !session.isModified() &&
                matches(volume(session), 4000 - one),
            "Open restores the fillet");
    std::filesystem::remove_all(directory);

    // M07: "Fase" uses the same panel with a distance; 1 mm removes 5 mm^3.
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    QAction& chamfer = command(window, "solid.modify.chamfer");
    require(chamfer.isEnabled(), "Fase offered for the selected box");
    chamfer.trigger();
    settle();
    require(panel(window) != nullptr &&
                guidance(window).contains(QStringLiteral("angefast werden")),
            "The chamfer panel opens and waits for an edge");
    click(*viewport, {edge.x, edge.y - 20, edge.z});
    require(guidance(window).contains(QStringLiteral("Abstand 1 mm")),
            "A clicked edge is chamfered with distance 1");
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 3 &&
                session.features().back().kind == document::FeatureKind::chamfer &&
                matches(volume(session), 4000 - one - 5),
            "OK chamfers the edge as a chamfer feature");
    std::cout << "PASS M06 fillet panel: edge pick, preview, radius, refusal, cancel, OK, "
                 "undo, edit, remove edge, open\n";
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
