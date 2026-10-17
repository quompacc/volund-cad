#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M24 (user decision 2026-10-14): a square of 10 on a plane 30 above XY, then a square of 20 on
// XY. Its region picked, "Ausformung" previews the ruled frustum (7000) to the other sketch;
// "Glatt" still gives a valid body; OK keeps "Ausformung 1"; undo.
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
bool matches(double a, double b) { return std::abs(a - b) < 1e-3 * std::max(1.0, std::abs(b)); }
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
double shown(viewport::CadViewport& viewport) {
    test::render(viewport);
    return viewport::ViewportTestAccess::displayedVolume(viewport);
}
document::FeatureId square(application::CadSession& session, document::SketchParameters sketch,
                           double half) {
    const document::SketchPoint corners[]{
        {-half, -half}, {half, -half}, {half, half}, {-half, half}};
    for (int index = 0; index < 4; ++index)
        sketch.elements.values.push_back(
            {document::SketchElementId{sketch.elements.nextId++},
             document::SketchLine{corners[index], corners[(index + 1) % 4]}});
    require(session.createDatumSketchFeature(sketch).status() ==
                application::CommandStatus::committed,
            "Sketch");
    return session.features().back().id;
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
    require(session.createDatumPlane(document::DatumPlane::xy, std::nullopt, std::nullopt, 30,
                                     document::DatumRotationAxis::none, 0)
                    .status() == application::CommandStatus::committed,
            "Plane");
    const auto plane = session.features().back().id;
    document::SketchParameters upper{*session.datumFrame(plane), {}};
    upper.constructionPlane = plane;
    static_cast<void>(square(session, upper, 5));
    document::SketchParameters base{document::datumSketchPlane(document::DatumPlane::xy), {}};
    base.datum = document::DatumPlane::xy;
    const auto profile = square(session, base, 10);
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    require(session.selectSketchRegions(profile, regions), "Region picked");
    window.refreshAfterExternalChange();
    test::render(*viewport);

    QAction& loft = command(window, "solid.create.loft");
    require(loft.isEnabled(), "Ausformung offered for a picked region");
    loft.trigger();
    settle();
    require(panel(window) != nullptr && matches(shown(*viewport), 7000),
            "The ruled frustum is previewed");
    auto* transition =
        panel(window)->findChild<QComboBox*>(QStringLiteral("toolChoice_transition"));
    require(transition != nullptr, "Transition choice");
    transition->setCurrentIndex(1);
    settle();
    require(shown(*viewport) > 0, "Glatt gives a body as well");
    panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().back().name == "Ausformung 1" && volume(session) > 0,
            "OK keeps the loft");
    command(window, "history.undo").trigger();
    settle();
    require(session.validBodies().empty(), "Undo removes it");
    std::cout << "PASS M24 loft panel: preview, smooth, OK, undo\n";
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
