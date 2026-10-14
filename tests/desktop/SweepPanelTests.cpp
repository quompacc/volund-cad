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
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M23 (user decision 2026-10-14): two path sketches on XZ (lines of 50 and 80 up Z), then a
// circle of radius 3 on XY. Its region picked, "Folgen" previews the sweep along the latest
// path (80); choosing the first path previews 50; OK keeps it as "Folgen 1"; undo.
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
document::FeatureId sketch(application::CadSession& session, document::DatumPlane plane,
                           document::SketchGeometry geometry) {
    document::SketchParameters parameters{document::datumSketchPlane(plane), {}};
    parameters.datum = plane;
    parameters.elements.values.push_back({document::SketchElementId{1}, std::move(geometry)});
    parameters.elements.nextId = 2;
    require(session.createDatumSketchFeature(parameters).status() ==
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
    static_cast<void>(
        sketch(session, document::DatumPlane::xz, document::SketchLine{{0, 0}, {0, 50}}));
    static_cast<void>(
        sketch(session, document::DatumPlane::xz, document::SketchLine{{0, 0}, {0, 80}}));
    const auto profile =
        sketch(session, document::DatumPlane::xy, document::SketchCircle{{0, 0}, 3});
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    require(session.selectSketchRegions(profile, regions), "Region picked");
    window.refreshAfterExternalChange();
    test::render(*viewport);

    QAction& sweep = command(window, "solid.create.sweep");
    require(sweep.isEnabled(), "Folgen offered for a picked region");
    sweep.trigger();
    settle();
    require(panel(window) != nullptr && matches(shown(*viewport), pi * 9 * 80),
            "The preview follows the latest path (80)");
    auto* path = panel(window)->findChild<QComboBox*>(QStringLiteral("toolChoice_path"));
    require(path != nullptr && path->count() == 2, "Two paths to choose");
    path->setCurrentIndex(0);
    settle();
    require(matches(shown(*viewport), pi * 9 * 50), "The first path previews 50");
    panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().back().name == "Folgen 1" &&
                matches(volume(session), pi * 9 * 50),
            "OK keeps the sweep");
    command(window, "history.undo").trigger();
    settle();
    require(session.validBodies().empty(), "Undo removes it");
    std::cout << "PASS M23 sweep panel: latest path, choose path, OK, undo\n";
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
