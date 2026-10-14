#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QStatusBar>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M27 (user decision 2026-10-14): "Kugel" on the selected box joins a sphere; the panel turns
// it into a cut ("Abziehen") and changes its radius; without a selection "Keil" adds a new
// body whose panel shows its four sizes; undo.
namespace {
using namespace voelund;
using test::require;
constexpr double pi = std::numbers::pi;

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
template <typename Widget> Widget& child(QWidget& window, const char* name) {
    auto* found = window.findChild<Widget*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}

void run() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 20}).feature();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    window.refreshAfterExternalChange();
    test::render(*viewport);

    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    QAction& sphere = command(window, "solid.create.sphere");
    require(sphere.isEnabled(), "Kugel offered");
    sphere.trigger();
    settle();
    require(session.features().back().kind == document::FeatureKind::primitive &&
                session.features().back().name == "Kugel 1" &&
                session.validBodies().size() == 1,
            "A sphere joined to the box");
    const double joined = volume(session);

    auto& mode = child<QComboBox>(window, "cylinderResultModeInput");
    mode.setCurrentIndex(1);
    emit mode.activated(1);
    settle();
    const double cap = pi * 4 * (3 * 10 - 2) / 3;
    require(matches(volume(session), 40 * 30 * 20 - cap) && joined > 40 * 30 * 20,
            "Abziehen cuts the cap away");
    auto& radius = child<QDoubleSpinBox>(window, "cylinderRadiusInput");
    radius.setValue(5);
    emit radius.editingFinished();
    settle();
    const double smallCap = pi * 4 * (3 * 5 - 2) / 3;
    require(matches(volume(session), 40 * 30 * 20 - smallCap), "Radius 5 in the panel");

    session.clearSelection();
    window.refreshAfterExternalChange();
    command(window, "solid.create.wedge").trigger();
    settle();
    require(session.features().back().name == "Keil 1" && session.validBodies().size() == 2,
            "A wedge as a new body");
    auto& topWidth = child<QDoubleSpinBox>(window, "primitiveSize3Input");
    auto* layout = static_cast<QFormLayout*>(topWidth.parentWidget()->layout());
    require(topWidth.isVisible() &&
                static_cast<QLabel*>(layout->labelForField(&topWidth))->text() ==
                    QStringLiteral("Oberbreite"),
            "The wedge panel shows its top width");
    require(matches(volume(session), 40 * 30 * 20 - smallCap + 20 * 15 * 20),
            "Box with cut plus wedge");

    command(window, "history.undo").trigger();
    settle();
    require(session.validBodies().size() == 1, "Undo removes the wedge");
    std::cout << "PASS M27 primitive panel: sphere joined, cut, radius, wedge, undo\n";
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
