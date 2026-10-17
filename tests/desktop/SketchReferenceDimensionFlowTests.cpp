#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S18 (user decision 2026-10-14): a line of length 12 in a sketch on XY; a click on its length
// label selects it, "Referenz umschalten" shows it in parentheses, again drives it, a third time
// makes it a reference again; OK stores the reference.
namespace {
using namespace voelund;
using test::require;
using viewport::ViewportTestAccess;

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
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    settle();
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
std::optional<viewport::SceneSketchConstraint> lengthLabel(viewport::CadViewport& viewport) {
    test::render(viewport);
    for (const auto& label : ViewportTestAccess::shownConstraints(viewport))
        if (label.label.find("L 12") != std::string::npos)
            return label;
    return std::nullopt;
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    click(*viewport, 30, 30, 0);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);

    command(window, "sketch.tool.line").trigger();
    settle();
    click(*viewport, 5, 5, 0);
    click(*viewport, 15, 5, 0);
    command(window, "sketch.tool.select").trigger();
    settle();
    click(*viewport, 10, 5, 0);
    auto* length = window.findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_line_length"));
    require(length != nullptr, "Length field");
    length->setValue(12);
    emit length->editingFinished();
    settle();

    QAction& toggle = command(window, "sketch.toggle_reference");
    auto label = lengthLabel(*viewport);
    require(label.has_value() && label->label.front() != '(', "A driving length label");
    // Into the text of the label, above the line it measures (an element wins over a label);
    // labels on one anchor stack downwards, so the rows are tried until the length is chosen.
    test::render(*viewport);
    const QPoint anchor = ViewportTestAccess::project(*viewport, label->x, label->y, label->z);
    for (int row = 0; row < 4 && !toggle.isEnabled(); ++row) {
        const QPoint text = anchor + QPoint(40, -10 + 18 * row);
        test::mouse(*viewport, QEvent::MouseMove, text, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, text, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, text, Qt::LeftButton, Qt::NoButton);
        settle();
    }
    require(toggle.isEnabled(), "The selected dimension can become a reference");
    toggle.trigger();
    settle();
    label = lengthLabel(*viewport);
    require(label.has_value() && label->label.front() == '(' && !label->editable,
            "The reference is shown in parentheses");
    toggle.trigger();
    settle();
    label = lengthLabel(*viewport);
    require(label.has_value() && label->label.front() != '(', "Driving again");
    toggle.trigger();
    settle();

    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    bool stored = false;
    for (const auto& constraint :
         session.sketchParameters(session.features().back().id).constraints.values)
        stored = stored || (constraint.reference && std::abs(constraint.value - 12) < 1e-6);
    require(stored, "OK stores the reference dimension");
    std::cout << "PASS S18 reference dimension flow: select label, toggle three times, OK\n";
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
