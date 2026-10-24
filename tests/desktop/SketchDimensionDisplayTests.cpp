// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../../src/viewport/SketchDimensionDrawing.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

// W12 (user report 2026-10-24, as in Fusion and FreeCAD): a sketch dimension is drawn as in a
// technical drawing - extension lines, a dimension line with arrows standing off the
// geometry and the value on it ("80 mm"), not a label at the line's middle. A click on the
// value still opens its field.
namespace {
using namespace voelund;
using namespace voelund::document;
void require(bool value, const std::string& message) {
    if (!value)
        throw std::runtime_error(message);
}

SketchParameters plate() {
    const auto line = [](std::uint64_t id, double x0, double y0, double x1, double y1) {
        return SketchElement{SketchElementId{id}, SketchLine{{x0, y0}, {x1, y1}}};
    };
    const auto ref = [](std::uint64_t id) {
        return SketchGeometryReference{SketchElementId{id}, SketchPointPosition::start};
    };
    SketchParameters sketch{datumSketchPlane(DatumPlane::xy), {}};
    sketch.datum = DatumPlane::xy;
    sketch.elements.values = {line(1, -40, -20, 40, -20),
                              line(2, 40, -20, 40, 20),
                              line(3, 40, 20, -40, 20),
                              line(4, -40, 20, -40, -20),
                              {SketchElementId{5}, SketchCircle{{0, 0}, 10}}};
    sketch.elements.nextId = 6;
    sketch.constraints.values = {
        {SketchConstraintId{1}, SketchConstraintKind::distance, ref(3), std::nullopt, 80.0},
        {SketchConstraintId{2}, SketchConstraintKind::distance, ref(2), std::nullopt, 40.0},
        {SketchConstraintId{3}, SketchConstraintKind::radius, ref(5), std::nullopt, 10.0}};
    SketchConstraint angle{SketchConstraintId{4}, SketchConstraintKind::angle, ref(1), ref(2), 0.0};
    angle.angleValue = kernel::Radians(std::numbers::pi / 2);
    sketch.constraints.values.push_back(angle);
    sketch.constraints.nextId = 5;
    return sketch;
}

viewport::SceneSketchConstraint shown(viewport::CadViewport& viewport, std::uint64_t id) {
    const auto constraints = viewport::ViewportTestAccess::shownConstraints(viewport);
    const auto found = std::ranges::find(constraints, id, &viewport::SceneSketchConstraint::id);
    require(found != constraints.end() && found->dimension.has_value(),
            "Dimension " + std::to_string(id) + " is drawn");
    return *found;
}

void run() {
    application::CadSession session;
    session.newDocument();
    voelund::test::requireCommittedFeature(session.createDatumSketchFeature(plate()));
    QMainWindow window;
    auto* host = new desktop::ToolPanelHost(&window);
    auto* viewport = new viewport::CadViewport(&window);
    window.setCentralWidget(viewport);
    desktop::SketchEditorToolController controller(
        session, *viewport, *host, [] {}, [](const QString&) {});
    viewport->setSketchConstraintEditHandler(
        [&controller](std::uint64_t id) { controller.editConstraint(id); });
    window.resize(1100, 800);
    window.show();
    test::render(*viewport);
    require(controller.beginSelectedSketch(), "begin sketch");
    viewport->setCameraView(viewport::CameraView::top);
    viewport->fitView();
    test::render(*viewport);

    const auto top = shown(*viewport, 1);
    require(top.dimension->text == "80 mm", "The length reads 80 mm: " + top.dimension->text);
    const auto topText = viewport::drawDimension(*top.dimension).text;
    require(topText.Y() > 20.5 && std::abs(topText.X()) < 1e-6,
            "The 80 mm dimension stands above the top side: y " + std::to_string(topText.Y()));
    const auto side = shown(*viewport, 2);
    const auto sideText = viewport::drawDimension(*side.dimension).text;
    require(side.dimension->text == "40 mm" && sideText.X() > 40.5,
            "The 40 mm dimension stands right of the right side: x " +
                std::to_string(sideText.X()));
    require(shown(*viewport, 3).dimension->text == "R 10 mm", "The radius reads R 10 mm");
    require(shown(*viewport, 4).dimension->text == "90°", "The angle reads 90°");
    const auto drawing = viewport::drawDimension(*top.dimension);
    require(drawing.segments.size() == 3 && drawing.arrows.size() == 2,
            "Two extension lines, a dimension line, two arrows");

    if (const auto directory = qEnvironmentVariable("VOELUND_DIMENSION_ARTIFACT_DIR");
        !directory.isEmpty())
        require(QDir().mkpath(directory) &&
                    viewport->grab().save(directory + "/sketch-dimensions.png"),
                "capture");

    // The value is still the place to double-click (as in Fusion): its field opens.
    const QPoint at =
        viewport::ViewportTestAccess::project(*viewport, topText.X(), topText.Y(), topText.Z()) +
        QPoint(0, -10);
    for (const auto type : {QEvent::MouseMove, QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                            QEvent::MouseButtonDblClick}) {
        QMouseEvent event(type, QPointF(at), QPointF(viewport->mapToGlobal(at)), Qt::LeftButton,
                          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(viewport, &event);
    }
    QApplication::processEvents();
    auto* field = viewport->findChild<QLineEdit*>(QStringLiteral("dimensionValueEdit"));
    require(field != nullptr && !field->isHidden(), "A double click on the value opens its field");
    std::cout << "PASS W12 sketch dimensions: extension lines, arrows, value on the line\n";
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
