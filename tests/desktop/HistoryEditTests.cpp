#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M13 (user request 2026-10-16: "mach mit M13"): a plate 20 x 20 x 10 with one top edge rounded
// by 4. The timeline reopens the early extrusion; 3 high, the fillet no longer fits: OK waits,
// and the panel offers "Mit Folgefehlern übernehmen". Cancel leaves the document untouched;
// keeping the change commits it with the fillet in error, nothing is deleted; undo restores the
// plate.
// M14 (user decision 2026-10-16): "Unterdrücken" in the fillet's context bar suppresses it (the
// plate keeps its full volume, the timeline step is marked suppressed, the command now says
// "Aktivieren"); the timeline step's right-click menu reactivates it. "Zurückrollen bis hier"
// from the extrusion step's menu leaves the fillet pale behind the marker, without an unsaved
// change; a click on the end marker rolls forward.
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
void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& button(QWidget& window, const char* name) {
    auto* found = panel(window)->findChild<QPushButton*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}
QPushButton& accept(QWidget& window) { return button(window, "toolAcceptButton"); }
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
double plateVolume(double height) {
    return 400 * height - 20 * 16 * (1 - std::numbers::pi / 4);
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
    document::SketchParameters square{document::datumSketchPlane(document::DatumPlane::xy), {}};
    square.datum = document::DatumPlane::xy;
    square.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 20}});
    square.elements.nextId = 2;
    require(session.createDatumSketchExtrusion(
                    square, features::identifySketchRegions(square.elements.values).ids, 10)
                    .status() == application::CommandStatus::committed,
            "Plate 20 x 20 x 10");
    const auto extrusion = session.features().back().id;
    std::optional<document::FilletEdge> edge;
    for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (std::abs(middle.Y()) < 1e-9 && std::abs(middle.Z() - 10) < 1e-9)
            edge = session.filletEdge(extrusion, explorer.Current());
    }
    require(edge && session.createFillet(extrusion, std::span{&*edge, 1}, 4).status() ==
                        application::CommandStatus::committed,
            "Top edge rounded by 4");
    require(matches(volume(session), plateVolume(10)), "Plate with its fillet");
    window.refreshAfterExternalChange();
    const auto revision = session.sceneStatus().document;

    const auto reopen = [&] {
        timelineStep(window, 1).click();
        settle();
        require(panel(window) != nullptr, "The timeline reopens the extrusion");
        field<QDoubleSpinBox>(window, QStringLiteral("toolLength_distance"))->setValue(3);
        settle();
    };
    reopen();
    auto* keep = panel(window)->findChild<QPushButton*>(QStringLiteral("keepSketchErrorsButton"));
    require(!accept(window).isEnabled() && keep != nullptr && keep->isVisible(),
            "3 high the fillet no longer fits: OK waits, keeping with errors is offered");
    button(window, "toolCancelButton").click();
    settle();
    require(panel(window) == nullptr && session.sceneStatus().document == revision &&
                matches(volume(session), plateVolume(10)),
            "Cancel leaves the whole chain as it was");

    reopen();
    panel(window)->findChild<QPushButton*>(QStringLiteral("keepSketchErrorsButton"))->click();
    settle();
    bool filletFails = false;
    for (const auto& state : session.featureStates())
        filletFails = filletFails || state.status != application::FeatureRuntimeStatus::current;
    require(panel(window) == nullptr && session.features().size() == 3 && filletFails &&
                session.identifiedSnapshot().snapshot().extrusionParameters(extrusion).distance == 3,
            "Kept with the fillet in error; nothing is deleted");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), plateVolume(10)),
            "Undo restores the plate with its fillet");

    // M14: suppress from the context bar, reactivate from the timeline menu.
    const auto fillet = session.features().back().id;
    require(session.selectFeature(fillet), "Fillet selected");
    window.refreshAfterExternalChange();
    QAction* suppress = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QStringLiteral("history.suppress"))
            suppress = action;
    require(suppress != nullptr && suppress->isEnabled() &&
                suppress->text() == QStringLiteral("Unterdrücken"),
            "Unterdrücken offered for the fillet");
    suppress->trigger();
    settle();
    require(matches(volume(session), 4000) && timelineStep(window, 2).property("featureState") ==
                                                  QStringLiteral("suppressed") &&
                suppress->text() == QStringLiteral("Aktivieren"),
            "Suppressed: full plate, step marked, the command reactivates");
    // The menu of the step: its action is triggered as a user would pick it.
    QTimer::singleShot(0, [] {
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* menu = qobject_cast<QMenu*>(widget); menu && menu->isVisible()) {
                for (auto* action : menu->actions())
                    if (action->text() == QStringLiteral("Aktivieren"))
                        action->trigger();
                menu->close();
            }
    });
    auto& step = timelineStep(window, 2);
    emit step.customContextMenuRequested(step.rect().center());
    settle();
    require(matches(volume(session), plateVolume(10)) &&
                timelineStep(window, 2).property("featureState").toString().isEmpty(),
            "The timeline menu reactivates the fillet");

    // M14: roll back from the extrusion step's menu, forward with the end marker.
    const auto pick = [](const QString& text) {
        QTimer::singleShot(0, [text] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* menu = qobject_cast<QMenu*>(widget); menu && menu->isVisible()) {
                    for (auto* action : menu->actions())
                        if (action->text() == text && action->isEnabled())
                            action->trigger();
                    menu->close();
                }
        });
    };
    const bool modified = session.isModified();
    pick(QStringLiteral("Zurückrollen bis hier"));
    auto& extrusionStep = timelineStep(window, 1);
    emit extrusionStep.customContextMenuRequested(extrusionStep.rect().center());
    settle();
    auto* end = window.findChild<QToolButton*>(QStringLiteral("timelineEnd"));
    require(session.rollbackAfter() == extrusion && matches(volume(session), 4000) &&
                timelineStep(window, 2).property("featureState") == QStringLiteral("rolledBack") &&
                session.isModified() == modified && end != nullptr && end->isEnabled(),
            "Rolled back after the extrusion: the fillet is pale, no unsaved change");
    end->click();
    settle();
    require(!session.rollbackAfter() && matches(volume(session), plateVolume(10)) &&
                !window.findChild<QToolButton*>(QStringLiteral("timelineEnd"))->isEnabled(),
            "The end marker rolls forward");
    std::cout << "PASS M13/M14 history edit: early extrusion reopened, successor error offered, "
                 "cancel unchanged, kept with errors, nothing deleted, undo; suppress from the "
                 "context bar, reactivate from the timeline menu; roll back from the menu, forward at "
                 "the end marker" << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
