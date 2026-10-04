#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// AI02d, A-ACTION-01: the shell builds action requests. The GUI flow (sketch on XY,
// extrusion, distance edit in the properties card, box and cylinder commands) and the same
// requests called directly on a second session give the same document and feature states.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::Quantity;
using application::QuantityUnit;
using test::require;

double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
}
Quantity mm(double value) { return {value, QuantityUnit::millimetre}; }

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
QPushButton& panelButton(QWidget& window, const char* name) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* button = panel == nullptr ? nullptr : panel->findChild<QPushButton*>(QLatin1String(name));
    require(button != nullptr, "Tool panel button missing");
    return *button;
}

// The GUI flow; returns the sketch and extrusion IDs it created.
std::pair<document::FeatureId, document::FeatureId> guiFlow(application::CadSession& session) {
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);

    click(*viewport, 30, 30);
    require(window.statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "Pick XY");
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -40, -20);
    click(*viewport, 40, 20);
    command(window, "sketch.tool.circle").trigger();
    click(*viewport, 0, 0);
    click(*viewport, 10, 0);
    panelButton(window, "toolAcceptButton").click();
    settle();
    test::render(*viewport);
    require(session.features().size() == 1, "Sketch created through the editor");
    const auto sketch = session.features().front().id;

    click(*viewport, 0, 20);
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    QDoubleSpinBox* distance = nullptr;
    for (auto* field : panel->findChildren<QDoubleSpinBox*>())
        if (field->isVisible())
            distance = field;
    require(distance != nullptr, "Extrusion distance field");
    distance->setValue(8.0);
    panelButton(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2, "Extrusion through the tool");
    const auto extrusion = session.features().back().id;

    auto* properties = window.findChild<QWidget*>(QStringLiteral("featurePropertiesCard"));
    require(properties != nullptr && properties->isVisible(), "Extrusion properties shown");
    auto* edit = properties->findChild<QDoubleSpinBox*>(QStringLiteral("extrusionDistanceInput"));
    edit->setValue(5.0);
    emit edit->editingFinished();
    settle();

    command(window, "solid.create.box").trigger();
    settle();
    command(window, "solid.create.cylinder").trigger();
    settle();
    require(session.features().size() == 4, "Box and cylinder through the commands");
    return {sketch, extrusion};
}

void sameResults() {
    application::CadSession gui;
    gui.newDocument();
    const auto [sketch, extrusion] = guiFlow(gui);
    const auto guiDocument = gui.identifiedSnapshot();

    application::CadSession direct;
    direct.newDocument();
    const auto run = [&](application::Action action, const std::string& step) {
        const auto result = application::executeAction(
            direct, application::prepareRequest(direct, std::move(action)));
        require(result.command.status() == CommandStatus::committed,
                (step + ": " + result.command.diagnostic()).c_str());
        return result;
    };
    run(application::CreateDatumSketchAction{gui.sketchParameters(sketch)}, "Sketch");
    const auto edited = guiDocument.snapshot().extrusionParameters(extrusion);
    run(application::CreateProfileExtrusionAction{sketch, edited.regions, mm(8)}, "Extrusion");
    run(application::EditExtrusionParametersAction{extrusion, edited}, "Distance");
    run(application::CreateBoxAction{mm(20), mm(20), mm(40)}, "Box");
    run(application::CreateCylinderAction{direct.features().back().id,
                                          document::CylinderResultMode::join},
        "Cylinder");

    const auto& a = guiDocument.snapshot();
    const auto b = direct.identifiedSnapshot();
    require(std::ranges::equal(a.features(), b.snapshot().features()) &&
                std::ranges::equal(a.parameters(), b.snapshot().parameters()) &&
                a.parameterTable() == b.snapshot().parameterTable(),
            "GUI and direct requests give the same document content");
    const auto guiStates = gui.featureStates();
    const auto directStates = direct.featureStates();
    require(std::ranges::equal(guiStates, directStates,
                               [](const auto& left, const auto& right) {
                                   return left.id == right.id && left.status == right.status;
                               }) &&
                std::ranges::all_of(guiStates,
                                    [](const auto& state) {
                                        return state.status ==
                                               application::FeatureRuntimeStatus::current;
                                    }),
            "GUI and direct requests give the same current feature states");
    require(std::abs(volume(gui) - volume(direct)) < 1e-6 * std::max(1.0, volume(gui)),
            "GUI and direct requests give the same volume");
    std::cout << "PASS AI02d: GUI and direct requests give equal results\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        sameResults();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
