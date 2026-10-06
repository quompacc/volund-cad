#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// T02: the extrusion panel offers direction and result (ADR 0012 target, user decision
// 2026-10-06). A face sketch defaults to join; choosing cut turns the direction into the
// material and makes a pocket. A base-plane sketch defaults to a new body and, with more than
// one body, names the body a join works on. VOELUND_EXTRUSION_PANEL_ARTIFACT_DIR keeps
// captures of both panels for review.
namespace {
using namespace voelund;
using test::require;

double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
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
QWidget& panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    require(found != nullptr && found->isVisible(), "Tool panel shown");
    return *found;
}
// The newest visible field; earlier panels' fields may await deletion.
template <class Field> Field* field(QWidget& window, const QString& name) {
    Field* found = nullptr;
    for (auto* candidate : panel(window).findChildren<Field*>(name))
        if (candidate->isVisible())
            found = candidate;
    return found;
}
QComboBox& choice(QWidget& window, const char* id) {
    auto* box = field<QComboBox>(window, QStringLiteral("toolChoice_") + QLatin1String(id));
    require(box != nullptr, "Panel choice missing");
    return *box;
}
void setDistance(QWidget& window, double value) {
    QDoubleSpinBox* distance = nullptr;
    for (auto* candidate : panel(window).findChildren<QDoubleSpinBox*>())
        if (candidate->isVisible())
            distance = candidate;
    require(distance != nullptr, "Distance field");
    distance->setValue(value);
}
void acceptPanel(QWidget& window) {
    auto* accept = panel(window).findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(accept != nullptr && accept->isEnabled(), "OK enabled");
    accept->click();
}
void capture(QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_EXTRUSION_PANEL_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    require(QDir().mkpath(directory), "capture directory");
    settle();
    require(window.grab().save(directory + QLatin1Char('/') + QLatin1String(name)), "capture");
}
document::SketchParameters onXy(double x0, double y0, double x1, double y1) {
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{x0, y0, x1, y1}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    return sketch;
}
document::FeatureId plate(application::CadSession& session, double x0, double x1) {
    const auto result = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateSketchExtrusionAction{
                                  onXy(x0, -20, x1, 20), {}, {10, application::QuantityUnit::millimetre}}));
    require(result.createdFeatures.size() == 2, "Plate");
    return result.createdFeatures.back();
}
// Selects the sketch's only region and opens the extrusion tool on it.
void openExtrusion(QWidget& window, application::CadSession& session, document::FeatureId sketch) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Region selected");
    static_cast<desktop::MainWindow&>(window).refreshAfterExternalChange();
    QAction& extrude = command(window, "solid.create.extrude_profiles");
    require(extrude.isEnabled(), "Extrude offered");
    extrude.trigger();
    settle();
}

void pocketFromFace(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    const auto support = plate(session, -40, 40);
    const auto faces = session.supportFaces(support);
    const auto top = std::ranges::find_if(
        faces, [](const auto& face) { return face.plane.normalZ > 0.5 && face.plane.originZ > 9; });
    require(top != faces.end(), "Top face");
    auto sketch = session.sketchOnFace(support, top->origin);
    // A 20 x 10 rectangle in the middle of the face; the area is what counts.
    const double cx = (top->minX + top->maxX) / 2;
    const double cy = (top->minY + top->maxY) / 2;
    sketch.elements = {
        {{document::SketchElementId{1}, document::SketchRectangle{cx - 10, cy - 5, cx + 10, cy + 5}}},
        2};
    const auto created = session.createSketchFeature(support, sketch);
    require(created.feature().has_value(), "Face sketch");
    openExtrusion(window, session, *created.feature());
    require(choice(window, "operation").currentIndex() == 1 &&
                choice(window, "direction").currentIndex() == 0 &&
                field<QComboBox>(window, QStringLiteral("toolChoice_target")) == nullptr,
            "A face sketch defaults to join along the normal, without a target field");
    choice(window, "operation").setCurrentIndex(2);
    settle();
    require(choice(window, "direction").currentIndex() == 1,
            "Choosing cut shows the direction into the material");
    setDistance(window, 4);
    capture(window, "t02-face-cut.png");
    acceptPanel(window);
    settle();
    require(session.validBodies().size() == 1 && matches(volume(session), 32000 - 800),
            "OK makes a 4 mm pocket in the plate");
}

void joinFromBasePlane(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    static_cast<void>(plate(session, -40, 40));
    static_cast<void>(plate(session, 100, 120));
    const auto created = session.createDatumSketchFeature(onXy(30, -5, 60, 5));
    require(created.feature().has_value(), "Base-plane sketch");
    openExtrusion(window, session, *created.feature());
    auto& target = choice(window, "target");
    require(choice(window, "operation").currentIndex() == 0 && target.count() == 2 &&
                target.itemText(0) == QStringLiteral("Körper 1"),
            "A base-plane sketch defaults to a new body; two bodies to choose from");
    choice(window, "operation").setCurrentIndex(1);
    target.setCurrentIndex(0);
    setDistance(window, 10);
    capture(window, "t02-base-join.png");
    acceptPanel(window);
    settle();
    require(session.validBodies().size() == 2 &&
                matches(volume(session), 32000 + 2000 + 20 * 40 * 10),
            "OK joins the rectangle to body 1");
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    pocketFromFace(window, session);
    joinFromBasePlane(window, session);
    std::cout << "PASS T02 extrusion panel: defaults, cut direction, target body\n";
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
