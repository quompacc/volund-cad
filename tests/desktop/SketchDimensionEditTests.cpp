#include "IsolatedSettings.hpp"
#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../CommandResultAssertions.hpp"
#include <QApplication>
#include <QInputDialog>
#include <QLabel>
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
    const auto id = voelund::test::requireCommittedFeature(session.createSketchFeature(box, sketch));
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

    // First 30 (L2 cannot reach), then 7 after the reason came back.
    int dialogs = 0;
    QString secondPrompt;
    double secondValue = 0.0;
    std::function<void()> answer = [&] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!dialog) {
            QTimer::singleShot(10, answer);
            return;
        }
        ++dialogs;
        if (dialogs == 1) {
            dialog->setTextValue(QStringLiteral("30"));
            QTimer::singleShot(0, answer);
        } else {
            secondPrompt = dialog->labelText();
            secondValue = dialog->textValue().toDouble();
            dialog->setTextValue(QStringLiteral("7"));
        }
        dialog->accept();
    };
    QTimer::singleShot(0, answer);
    controller.editConstraint(3);
    require(dialogs == 2, "The refused value reopened the dialog");
    require(secondPrompt.contains("nicht lösen") && std::abs(secondValue - 30) < 1e-9,
            "The dialog shows the reason and keeps the entered value, got " +
                secondPrompt.toStdString());
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
