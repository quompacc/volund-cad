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
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStatusBar>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <optional>
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
void click(viewport::CadViewport& viewport, const Corner& at,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier, QPoint nudge = {}) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, at.x, at.y, at.z) + nudge;
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton,
                modifiers);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton,
                modifiers);
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
    // User report 2026-10-13: the card grows with its text; nothing hides behind a scroll.
    settle();
    auto* scroll = panel(window)->findChild<QScrollArea*>();
    require(scroll != nullptr && scroll->verticalScrollBar()->maximum() == 0,
            "The long explanation and the fields fit into the card");
    setRadius(window, 1);
    button(window, "toolCancelButton").click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 1 &&
                matches(volume(session), 4000),
            "Cancel keeps the box");

    // User request 2026-10-13: edges are picked before the tool, Ctrl adds one; "Verrunden"
    // takes them over.
    // Without a tool a click right on a silhouette edge may round onto its face; like the
    // user, the test clicks just outside the body there (U14: the body now fills the view).
    const auto outward = [&](const Corner& at) {
        const QPoint middle = viewport::ViewportTestAccess::project(*viewport, 10, 10, 5);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, at.x, at.y, at.z);
        const QPointF away(point - middle);
        const double length = std::max(1.0, std::hypot(away.x(), away.y()));
        return QPoint(qRound(2 * away.x() / length), qRound(2 * away.y() / length));
    };
    click(*viewport, edge, Qt::NoModifier, outward(edge));
    const Corner front{edge.x, edge.y - 20, edge.z};
    click(*viewport, front, Qt::ControlModifier, outward(front));
    QAction& preselected = command(window, "solid.modify.fillet");
    require(panel(window) == nullptr && preselected.isEnabled() &&
                window.statusBar()->currentMessage().startsWith(QStringLiteral("2 Kante")),
            "Two edges picked without a tool");
    preselected.trigger();
    settle();
    require(panel(window) != nullptr && guidance(window).startsWith(QStringLiteral("2 Kante")) &&
                matches(shown(*viewport), 4000 - 2 * one),
            "Verrunden takes over the picked edges");
    click(*viewport, {edge.x - 20, edge.y - 20, edge.z});
    require(guidance(window).startsWith(QStringLiteral("3 Kante")) &&
                matches(shown(*viewport), 4000 - 3 * one),
            "A third edge is added in the panel");
    button(window, "toolCancelButton").click();
    settle();
    require(matches(volume(session), 4000), "Cancel keeps the box");

    // User request 2026-10-13: a grip on the edge, dragged into the corner, sets the radius;
    // the preview follows while the button is down.
    openFillet(window, session, box);
    click(*viewport, edge);
    const double half = std::sqrt(0.5);
    const QPoint base =
        viewport::ViewportTestAccess::project(*viewport, edge.x - half, edge.y - half, edge.z);
    const QPoint inward = viewport::ViewportTestAccess::project(
        *viewport, edge.x - half - 15 * half, edge.y - half - 15 * half, edge.z);
    const QPointF direction =
        QPointF(inward - base) / std::hypot(inward.x() - base.x(), inward.y() - base.y());
    std::optional<QPoint> grip;
    for (int step = 0; step <= 120 && !grip; step += 2) {
        const QPoint candidate = base + (direction * step).toPoint();
        test::mouse(*viewport, QEvent::MouseMove, candidate, Qt::NoButton, Qt::NoButton);
        if (viewport::ViewportTestAccess::pushPullArrowHovered(*viewport))
            grip = candidate;
    }
    require(grip.has_value(), "The grip arrow sits on the chosen edge");
    const double before = shown(*viewport);
    test::mouse(*viewport, QEvent::MouseButtonPress, *grip, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseMove, *grip + (direction * 30).toPoint(), Qt::NoButton,
                Qt::LeftButton);
    QElapsedTimer waiting;
    waiting.start();
    while (waiting.elapsed() < 200)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    require(shown(*viewport) < before - 0.1, "The rounding grows while the grip is dragged");
    test::mouse(*viewport, QEvent::MouseButtonRelease, *grip + (direction * 30).toPoint(),
                Qt::LeftButton, Qt::NoButton);
    settle();
    QDoubleSpinBox* radius = nullptr;
    for (auto* candidate :
         panel(window)->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_radius")))
        if (candidate->isVisible())
            radius = candidate;
    require(radius != nullptr && radius->value() > 1.5 &&
                std::abs(radius->value() - std::round(radius->value())) < 1e-9,
            "Dragging the grip sets a larger radius in snapped steps");
    const double r = radius->value();
    require(matches(shown(*viewport), 4000 - 10 * r * r * (1 - pi / 4)),
            "The preview shows the dragged radius");
    button(window, "toolCancelButton").click();
    settle();

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
