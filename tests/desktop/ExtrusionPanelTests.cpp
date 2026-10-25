// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../integration/CadSessionTestSupport.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <numbers>
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
void setLength(QWidget& window, const char* id, double value) {
    auto* input = field<QDoubleSpinBox>(window, QStringLiteral("toolLength_") + QLatin1String(id));
    require(input != nullptr, "Length field");
    input->setValue(value);
}
void setDistance(QWidget& window, double value) { setLength(window, "distance", value); }
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
        session,
        application::prepareRequest(
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

QString guidance(QWidget& window) {
    auto* label = panel(window).findChild<QLabel*>(QStringLiteral("toolGuidance"));
    require(label != nullptr, "Guidance label");
    return label->text();
}
void cancelPanel(QWidget& window) {
    panel(window).findChild<QPushButton*>(QStringLiteral("toolCancelButton"))->click();
    settle();
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

// M04: fields that do not apply are off or hidden; the guidance names the result body; a cut
// turns the direction and leaving the cut turns it back.
void fieldRules(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    static_cast<void>(plate(session, -40, 40));
    const auto created = session.createDatumSketchFeature(onXy(-10, -5, 10, 5));
    require(created.feature().has_value(), "Base-plane sketch");
    openExtrusion(window, session, *created.feature());
    require(field<QComboBox>(window, QStringLiteral("toolChoice_target")) == nullptr &&
                guidance(window).contains(QStringLiteral("neuer Körper")),
            "A new body has no target field and says so");
    choice(window, "operation").setCurrentIndex(1);
    settle();
    require(guidance(window).contains(QStringLiteral("Ziel: Körper 1")),
            "A join names its only target body");
    choice(window, "direction").setCurrentIndex(2);
    settle();
    require(!field<QDoubleSpinBox>(window, QStringLiteral("toolLength_offset"))->isEnabled() &&
                !choice(window, "end").isEnabled(),
            "Symmetric has no offset and no end face");
    choice(window, "direction").setCurrentIndex(0);
    choice(window, "operation").setCurrentIndex(2);
    settle();
    require(choice(window, "direction").currentIndex() == 1, "A cut turns into the material");
    choice(window, "operation").setCurrentIndex(1);
    settle();
    require(choice(window, "direction").currentIndex() == 0,
            "Leaving the cut turns the direction back");
    cancelPanel(window);
}

// M04 (user decision 2026-10-12): "Bearbeiten" on an extrusion reopens its panel with its
// values; the preview follows the distance, Cancel keeps the document, OK is one undo step.
void editExtrusion(desktop::MainWindow& window, application::CadSession& session,
                   viewport::CadViewport& viewport) {
    session.newDocument();
    const auto extrusion = plate(session, -40, 40);
    require(session.selectFeature(extrusion), "Extrusion selected");
    window.refreshAfterExternalChange();
    QAction& edit = command(window, "solid.modify.edit_sketch");
    require(edit.isEnabled(), "Bearbeiten offered for the extrusion");
    edit.trigger();
    settle();
    auto* distance = field<QDoubleSpinBox>(window, QStringLiteral("toolLength_distance"));
    require(distance != nullptr && matches(distance->value(), 10),
            "The panel opens with the stored distance");
    setDistance(window, 12);
    settle();
    require(matches(viewport::ViewportTestAccess::displayedVolume(viewport), 80 * 40 * 12),
            "The preview shows 12 mm");
    cancelPanel(window);
    require(matches(volume(session), 32000), "Cancel keeps the document");
    require(session.selectFeature(extrusion), "Extrusion selected again");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.edit_sketch").trigger();
    settle();
    setDistance(window, 12);
    acceptPanel(window);
    settle();
    require(matches(volume(session), 80 * 40 * 12), "OK changes the extrusion to 12 mm");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 32000) && session.canUndo(),
            "One undo returns to 10 mm; the plate's creation is the step before");
    require(session.redo().status() == application::CommandStatus::committed &&
                matches(volume(session), 80 * 40 * 12),
            "Redo returns to 12 mm");
}

// M04 (user decision 2026-10-12): with the panel open, a click into a region toggles it.
void toggleRegions(desktop::MainWindow& window, application::CadSession& session,
                   viewport::CadViewport& viewport) {
    session.newDocument();
    auto sketch = onXy(-30, -5, -10, 5);
    sketch.elements = {{{document::SketchElementId{1}, document::SketchRectangle{-30, -5, -10, 5}},
                        {document::SketchElementId{2}, document::SketchRectangle{10, -5, 30, 5}}},
                       3};
    const auto created = session.createDatumSketchFeature(sketch);
    require(created.feature().has_value(), "Two-rectangle sketch");
    openExtrusion(window, session, *created.feature());
    require(guidance(window).startsWith(QStringLiteral("1 ")), "One region at first");
    click(viewport, 20, 0, 0);
    require(guidance(window).startsWith(QStringLiteral("2 ")) &&
                matches(viewport::ViewportTestAccess::displayedVolume(viewport), 2 * 2000),
            "A click adds the second region to the preview");
    click(viewport, 20, 0, 0);
    require(guidance(window).startsWith(QStringLiteral("1 ")) &&
                matches(viewport::ViewportTestAccess::displayedVolume(viewport), 2000),
            "A second click removes it again");
    click(viewport, 20, 0, 0);
    acceptPanel(window);
    settle();
    require(matches(volume(session), 2 * 2000), "OK extrudes both regions");
}

// M04: a region whose contour changed is explained in German and its elements turn red.
void invalidRegionExplained(desktop::MainWindow& window, application::CadSession& session,
                            viewport::CadViewport& viewport) {
    session.newDocument();
    const auto extrusion = plate(session, -40, 40);
    const auto sketch = session.features().front().id;
    auto split = session.sketchParameters(sketch);
    split.elements.values.push_back(
        {document::SketchElementId{2}, document::SketchLine{{0, -20}, {0, 20}}});
    split.elements.nextId = 3;
    static_cast<void>(session.editSketchParameters(
        sketch, split, application::ErrorDisposition::keepEditableErrors));
    require(session.selectFeature(extrusion), "Extrusion selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.edit_sketch").trigger();
    settle();
    test::render(viewport);
    const auto color = viewport::ViewportTestAccess::sketchElementColor(viewport, 1);
    require(guidance(window).contains(QStringLiteral("Kontur dieses Bereichs")) &&
                color.has_value() && color->Red() > 0.9 && color->Green() < 0.3,
            "The changed contour is explained and marked red");
    cancelPanel(window);
}

// A 20 x 10 rectangle in the middle of the plate's top face, as a sketch on that face.
document::FeatureId faceSketch(application::CadSession& session, document::FeatureId support) {
    const auto faces = session.supportFaces(support);
    const auto top = std::ranges::find_if(
        faces, [](const auto& face) { return face.plane.normalZ > 0.5 && face.plane.originZ > 9; });
    require(top != faces.end(), "Top face");
    auto sketch = session.sketchOnFace(support, top->origin);
    // A 20 x 10 rectangle in the middle of the face; the area is what counts.
    const double cx = (top->minX + top->maxX) / 2;
    const double cy = (top->minY + top->maxY) / 2;
    sketch.elements = {{{document::SketchElementId{1},
                         document::SketchRectangle{cx - 10, cy - 5, cx + 10, cy + 5}}},
                       2};
    const auto created = session.createSketchFeature(support, sketch);
    require(created.feature().has_value(), "Face sketch");
    return *created.feature();
}

void pocketFromFace(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    openExtrusion(window, session, faceSketch(session, plate(session, -40, 40)));
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
    require(choice(window, "operation").currentIndex() == 0,
            "A base-plane sketch defaults to a new body");
    choice(window, "operation").setCurrentIndex(1);
    settle();
    auto& target = choice(window, "target");
    require(target.count() == 2 && target.itemText(0) == QStringLiteral("Körper 1"),
            "A join offers the two bodies");
    target.setCurrentIndex(0);
    setDistance(window, 10);
    capture(window, "t02-base-join.png");
    acceptPanel(window);
    settle();
    require(session.validBodies().size() == 2 &&
                matches(volume(session), 32000 + 2000 + 20 * 40 * 10),
            "OK joins the rectangle to body 1");
}

// M02 (user decisions 2026-10-11): "Symmetrisch" takes the distance as total length and
// "Schneiden" keeps what the prism and the plate share: a 20 x 10 rectangle, 6 mm symmetric
// about z = 0, intersected with the plate (z 0..10) leaves 20 x 10 x 3.
void symmetricIntersect(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    static_cast<void>(plate(session, -40, 40));
    const auto created = session.createDatumSketchFeature(onXy(-10, -5, 10, 5));
    require(created.feature().has_value(), "Base-plane sketch");
    openExtrusion(window, session, *created.feature());
    require(choice(window, "direction").itemText(2) == QStringLiteral("Symmetrisch") &&
                choice(window, "operation").itemText(3) == QStringLiteral("Schneiden"),
            "The panel offers Symmetrisch and Schneiden");
    choice(window, "direction").setCurrentIndex(2);
    choice(window, "operation").setCurrentIndex(3);
    setDistance(window, 6);
    acceptPanel(window);
    settle();
    require(session.validBodies().size() == 1 && matches(volume(session), 20 * 10 * 3),
            "OK keeps the 600 mm^3 the symmetric prism shares with the plate");
}

// W16 (user request 2026-10-25): the taper angle leans the sides; symmetric locks it.
void taperAngle(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    const auto created = session.createDatumSketchFeature(onXy(-10, -10, 10, 10));
    require(created.feature().has_value(), "Base-plane sketch");
    openExtrusion(window, session, *created.feature());
    auto* taper = field<QDoubleSpinBox>(window, QStringLiteral("toolLength_taper"));
    require(taper != nullptr && taper->isEnabled(), "The panel offers the taper angle");
    choice(window, "direction").setCurrentIndex(2);
    require(!taper->isEnabled(), "Symmetric locks the taper");
    choice(window, "direction").setCurrentIndex(0);
    setDistance(window, 10);
    setLength(window, "taper", -10);
    acceptPanel(window);
    settle();
    const double top = 20 - 20 * std::tan(10 * std::numbers::pi / 180);
    require(
        std::abs(volume(session) - 10.0 / 3 * (400 + top * top + 20 * top)) < 1e-3,
        (std::string("OK keeps the narrowing prism: ") + std::to_string(volume(session))).c_str());
}

// M03: a join that does not touch its target says so in the panel and keeps OK disabled.
void separateJoinExplained(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    static_cast<void>(plate(session, -40, 40));
    const auto created = session.createDatumSketchFeature(onXy(200, 0, 210, 10));
    require(created.feature().has_value(), "Far base-plane sketch");
    openExtrusion(window, session, *created.feature());
    choice(window, "operation").setCurrentIndex(1);
    settle();
    auto* guidance = panel(window).findChild<QLabel*>(QStringLiteral("toolGuidance"));
    auto* accept = panel(window).findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(guidance != nullptr && accept != nullptr && !accept->isEnabled() &&
                guidance->text().contains(QStringLiteral("berührt den Zielkörper nicht")),
            "A separate join is explained and cannot be accepted");
    panel(window).findChild<QPushButton*>(QStringLiteral("toolCancelButton"))->click();
    settle();
}

// T03: a start offset lifts a new body; "Bis Fläche" waits for a face picked in the
// viewport, then ends the column on the top of a box above the plate.
void offsetAndEndFace(desktop::MainWindow& window, application::CadSession& session,
                      viewport::CadViewport& viewport) {
    session.newDocument();
    auto support = plate(session, -40, 40);
    openExtrusion(window, session, faceSketch(session, support));
    choice(window, "operation").setCurrentIndex(0);
    setDistance(window, 8);
    setLength(window, "offset", 5);
    acceptPanel(window);
    settle();
    require(session.validBodies().size() == 2 &&
                std::abs(test::maximumZOf(session.currentShape()) - 23) < 1e-6,
            "Offset 5 lifts the new body to z = 15..23");

    session.newDocument();
    support = plate(session, -40, 40);
    const auto box = session.createBoxFeature();
    require(box.feature().has_value(), "Box");
    auto placed = session.identifiedSnapshot().snapshot().boxParameters(*box.feature());
    placed.dimensions = {30, 30, 5};
    placed.placement.translationX = -15;
    placed.placement.translationY = -5;
    placed.placement.translationZ = 30;
    require(session.editBoxParameters(*box.feature(), placed).status() ==
                application::CommandStatus::committed,
            "Box above the plate");
    openExtrusion(window, session, faceSketch(session, support));
    choice(window, "end").setCurrentIndex(1);
    settle();
    auto* accept = panel(window).findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(!accept->isEnabled() &&
                !field<QDoubleSpinBox>(window, QStringLiteral("toolLength_distance"))->isEnabled(),
            "Up to face waits for a face; the distance is off");
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, 10, 20, 35);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
    require(accept->isEnabled(), "A picked face enables OK");
    capture(window, "t03-up-to-face.png");
    acceptPanel(window);
    settle();
    const auto plateBody = std::ranges::find(session.validBodies(), document::BodyId{1},
                                             &features::EvaluatedBody::bodyId);
    GProp_GProps properties;
    BRepGProp::VolumeProperties(plateBody->shape, properties);
    require(matches(properties.Mass(), 32000 + 20 * 10 * 25),
            "The column joins the plate and ends on the box top at z = 35");
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
    symmetricIntersect(window, session);
    taperAngle(window, session);
    separateJoinExplained(window, session);
    fieldRules(window, session);
    viewport->setCameraView(viewport::CameraView::isometric);
    offsetAndEndFace(window, session, *viewport);
    editExtrusion(window, session, *viewport);
    toggleRegions(window, session, *viewport);
    invalidRegionExplained(window, session, *viewport);
    std::cout << "PASS T02/T03/M04 extrusion panel: defaults, field rules, target, offset, up to "
                 "face, edit, region toggle, invalid region\n";
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
