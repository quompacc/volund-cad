// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <cmath>
#include <functional>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

// S07: a dimension the sketch cannot satisfy comes back in the dialog with its reason and the
// entered value, ready to be corrected; the corrected value is applied as one step.
namespace {
using namespace voelund;
using namespace voelund::document;
void require(bool value, const std::string& message) {
    if (!value)
        throw std::runtime_error(message);
}

void run() {
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    // L1 fixed on the x axis; L2 from its start, 5 long, its end sqrt(125) from L1's end.
    const SketchParameters sketch{
        {0, 0, 20, 0, 0, 1, 1, 0, 0},
        {{{SketchElementId{1}, SketchLine{{0, 0}, {10, 0}}},
          {SketchElementId{2}, SketchLine{{0, 0}, {0, 5}}}},
         3},
        {{{SketchConstraintId{1}, SketchConstraintKind::fixed,
           SketchGeometryReference{SketchElementId{1}, SketchPointPosition::start}, std::nullopt,
           0.0, std::nullopt, std::vector<double>{0, 10, 0}},
          {SketchConstraintId{2}, SketchConstraintKind::coincident,
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::start},
           SketchGeometryReference{SketchElementId{1}, SketchPointPosition::start}},
          {SketchConstraintId{3}, SketchConstraintKind::distance,
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::start}, std::nullopt,
           5.0},
          {SketchConstraintId{4}, SketchConstraintKind::pointDistance,
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::end},
           SketchGeometryReference{SketchElementId{1}, SketchPointPosition::end},
           std::sqrt(125.0)}},
         5}};
    const auto id =
        voelund::test::requireCommittedFeature(session.createSketchFeature(box, sketch));
    QMainWindow window;
    auto* host = new desktop::ToolPanelHost(&window);
    viewport::CadViewport viewport;
    QString status;
    desktop::SketchEditorToolController controller(
        session, viewport, *host, [] {}, [&](const QString& message) { status = message; });
    require(controller.beginSelectedSketch(), "begin sketch");
    window.resize(1000, 800);
    window.show();
    QApplication::processEvents();

    // D02: the value field opens at the pointer. First 30 (L2 cannot reach), then 7 after the
    // reason came back.
    controller.editConstraint(3);
    auto* field = viewport.findChild<QLineEdit*>(QStringLiteral("dimensionValueEdit"));
    require(field != nullptr && !field->isHidden(), "The value field opens in the viewport");
    const auto enter = [&](const QString& text) {
        field->setText(text);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(field, &press);
        QApplication::processEvents();
    };
    enter(QStringLiteral("30"));
    require(!field->isHidden() && field->text() == QStringLiteral("30") &&
                status.contains("nicht lösen"),
            "A refused value keeps the field open with the value and the reason, got " +
                status.toStdString());
    enter(QStringLiteral("7"));
    require(field->isHidden(), "The applied value closes the field");
    require(status.contains("7.000"), "The corrected value was applied: " + status.toStdString());

    QPushButton* accept = nullptr;
    for (auto* button : host->findChildren<QPushButton*>())
        if (button->objectName() == "toolAcceptButton")
            accept = button;
    require(accept && accept->isEnabled(), "Sketch can be accepted");
    accept->click();
    const auto changed = session.sketchParameters(id);
    const auto& line = std::get<SketchLine>(changed.elements.values[1].geometry);
    require(std::abs(std::hypot(line.end.x - line.start.x, line.end.y - line.start.y) - 7) < 1e-7,
            "L2 is 7 long");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.sketchParameters(id) == sketch,
            "One undo step restores the sketch");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        std::cout << "PASS sketch dimension edit\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
