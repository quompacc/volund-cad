// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S13: the four switches of the sketch editor change the named state, from the panel and from
// their commands alike, and none of them marks the document as changed: grid, dimension
// labels, projected geometry (drawn and snapped) and automatic relations.
namespace {
using namespace voelund;
using viewport::ViewportTestAccess;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

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
    const QPoint point = ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
QComboBox& choice(QWidget& window, const char* id) {
    auto* input = window.findChild<QComboBox*>(QStringLiteral("toolChoice_") + QLatin1String(id));
    require(input != nullptr, std::string("Panel choice ") + id);
    return *input;
}

void run() {
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature({40, 20, 30}).status() ==
                application::CommandStatus::committed,
            "Box");
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    const auto stamp = session.sceneStatus().document;

    // Grid: a command outside the sketch, in step with the panel inside it.
    command(window, "view.grid").trigger();
    settle();
    require(!ViewportTestAccess::gridVisible(*viewport) && session.sceneStatus().document == stamp,
            "view.grid hides the grid without changing the document");
    command(window, "view.grid").trigger();
    settle();

    // A sketch on the box's top face shows its edges as projected geometry.
    session.clearSelection();
    click(*viewport, 20, 10, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    require(ViewportTestAccess::projectedEdges(*viewport) > 0, "Projected face edges shown");

    choice(window, "show_grid").setCurrentIndex(1);
    settle();
    require(!ViewportTestAccess::gridVisible(*viewport) &&
                !command(window, "view.grid").isChecked(),
            "Panel grid off: grid hidden, command unchecked");
    command(window, "view.grid").trigger();
    settle();
    require(ViewportTestAccess::gridVisible(*viewport) &&
                choice(window, "show_grid").currentIndex() == 0,
            "Command grid on: panel follows");

    command(window, "sketch.show_projected").trigger();
    settle();
    require(ViewportTestAccess::projectedEdges(*viewport) == 0 &&
                choice(window, "show_projected").currentIndex() == 1,
            "Projected geometry hidden by its command; the panel follows");
    choice(window, "show_projected").setCurrentIndex(0);
    settle();
    require(ViewportTestAccess::projectedEdges(*viewport) > 0 &&
                command(window, "sketch.show_projected").isChecked(),
            "Projected geometry back from the panel; the command follows");

    // A line with a length: one dimension label among the relations.
    command(window, "sketch.tool.line").trigger();
    click(*viewport, 5, 5, 30);
    click(*viewport, 15, 5, 30);
    command(window, "sketch.tool.select").trigger();
    click(*viewport, 10, 5, 30);
    auto* length = window.findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_line_length"));
    require(length != nullptr, "Length field");
    length->setValue(12);
    emit length->editingFinished();
    settle();
    test::render(*viewport);
    const auto labels = ViewportTestAccess::constraintLabels(*viewport);
    command(window, "sketch.show_dimensions").trigger();
    settle();
    require(ViewportTestAccess::constraintLabels(*viewport) == labels - 1,
            "Dimensions hidden, relations stay");
    command(window, "sketch.show_dimensions").trigger();
    settle();
    require(ViewportTestAccess::constraintLabels(*viewport) == labels, "Dimensions back");

    command(window, "sketch.auto_relations").trigger();
    settle();
    require(choice(window, "relations").currentIndex() == 1 &&
                !command(window, "sketch.auto_relations").isChecked(),
            "Automatic relations off through their command");
    require(session.features().size() == 1, "No switch added anything to the document");
    std::cout << "PASS sketch view switches: grid, dimensions, projected, relations\n";
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
