// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/ModelTreePanel.hpp"
#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"
#include "ViewportTestAccess.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// N02b (user decision 2026-10-21): three boxes apart in X. A plain click on the first body row,
// Strg-clicks on the other two: "3 Körper gewählt". The move handle drags all three as one undo
// step. Strg+C/Strg+V pastes three copies at once. In the timeline a plain click and a
// Strg-click group two steps; "Unterdrücken" suppresses both, undo brings both back; Löschen
// with a group of bodies removes all three, one undo restores them.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    Window() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    QAction& command(const char* id) {
        for (auto* action : main->findChildren<QAction*>())
            if (action->property("commandId").toString() == QLatin1String(id))
                return *action;
        throw std::runtime_error(std::string("Command missing: ") + id);
    }
    void property(const char* name, double value) {
        auto* input = main->findChild<QDoubleSpinBox*>(QLatin1String(name));
        require(input != nullptr && input->isVisible(), std::string("Property ") + name);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    void click(QWidget& widget, QPoint at, Qt::KeyboardModifiers modifiers) {
        const QPointF global = widget.mapToGlobal(QPointF(at));
        for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent event(type, QPointF(at), global, Qt::LeftButton,
                              type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                              modifiers);
            QApplication::sendEvent(&widget, &event);
        }
        test::settleEvents();
    }
    void clickBodyRow(int index, Qt::KeyboardModifiers modifiers) {
        auto* tree = main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
        using Panel = desktop::ModelTreePanel;
        int seen = 0;
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if ((*it)->data(0, Panel::kindRole).toInt() == static_cast<int>(Panel::Kind::body) &&
                seen++ == index)
                return click(*tree->viewport(), tree->visualItemRect(*it).center(), modifiers);
        throw std::runtime_error("Body row missing");
    }
    void clickStep(int index, Qt::KeyboardModifiers modifiers) {
        int seen = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (step->isVisible() && seen++ == index) {
                click(*step, step->rect().center(), modifiers);
                return;
            }
        throw std::runtime_error("Timeline step missing");
    }
    void key(int code, Qt::KeyboardModifiers modifiers) {
        test::activate(*main);
        QKeyEvent press(QEvent::KeyPress, code, modifiers);
        QApplication::sendEvent(viewport, &press);
        test::settleEvents();
        test::render(*viewport);
    }
    [[nodiscard]] double minimumX() const {
        Bnd_Box box;
        for (const auto& body : session.validBodies())
            BRepBndLib::Add(body.shape, box);
        double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
        box.Get(x0, y0, z0, x1, y1, z1);
        return x0;
    }
    [[nodiscard]] QString status() const { return main->statusBar()->currentMessage(); }
    // A-VIS-01: a picture when VOELUND_UI_FLOW_ARTIFACT_DIR is set.
    void capture(const char* name) {
        const QString directory = qEnvironmentVariable("VOELUND_UI_FLOW_ARTIFACT_DIR");
        if (directory.isEmpty())
            return;
        test::render(*viewport);
        main->grab().save(directory + QStringLiteral("/n02-") + QLatin1String(name) +
                          QStringLiteral(".png"));
    }
};

void threeBoxes(Window& w) {
    for (const double x : {0.0, 30.0, 60.0}) {
        w.click(*w.viewport, viewport::ViewportTestAccess::project(*w.viewport, -200, -200, 0),
                Qt::NoModifier); // nothing selected: a new body
        test::useRailTool(w.child("toolRail"), "solid.create.box");
        w.property("boxTranslation0Input", x);
    }
    require(w.session.validBodies().size() == 3, "Three boxes");
}

void moveTogether(Window& w) {
    w.clickBodyRow(0, Qt::NoModifier);
    w.clickBodyRow(1, Qt::ControlModifier);
    w.clickBodyRow(2, Qt::ControlModifier);
    require(w.status() == QStringLiteral("3 Körper gewählt"), "Group: " + w.status().toStdString());
    w.capture("body-group");
    w.viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*w.viewport);
    w.command("solid.modify.transform").trigger();
    test::render(*w.viewport);
    // The handle sits on the leading (last chosen) box; looked for at each box.
    std::optional<QPoint> handle;
    for (const double x : {65.0, 35.0, 5.0})
        if (!handle)
            handle = viewport::ViewportTestAccess::translationHandle(*w.viewport, {x, 5, 5});
    require(handle.has_value(), "The move handle is shown for the group");
    const double before = w.minimumX();
    const auto steps = w.session.features().size();
    const QPoint to = viewport::ViewportTestAccess::translationDestination(*w.viewport, *handle);
    test::mouse(*w.viewport, QEvent::MouseMove, *handle, Qt::NoButton, Qt::NoButton);
    test::mouse(*w.viewport, QEvent::MouseButtonPress, *handle, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 20; ++step)
        test::mouse(*w.viewport, QEvent::MouseMove, *handle + (to - *handle) * step / 20,
                    Qt::NoButton, Qt::LeftButton);
    test::mouse(*w.viewport, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    test::render(*w.viewport);
    const double after = w.minimumX();
    require(std::abs(after - before) > 1e-3 && w.session.validBodies().size() == 3,
            "The three boxes moved together");
    w.command("solid.modify.transform").trigger(); // the tool off again
    require(w.session.undo().status() == application::CommandStatus::committed &&
                std::abs(w.minimumX() - before) < 1e-9 && w.session.features().size() == steps,
            "One undo moves all three back");
    w.main->refreshAfterExternalChange();
}

void copyThree(Window& w) {
    w.clickBodyRow(0, Qt::NoModifier);
    w.clickBodyRow(2, Qt::ShiftModifier);
    require(w.status() == QStringLiteral("3 Körper gewählt"),
            "Umschalt-click groups the range: " + w.status().toStdString());
    w.key(Qt::Key_C, Qt::ControlModifier);
    w.key(Qt::Key_V, Qt::ControlModifier);
    require(w.session.validBodies().size() == 6, "Three copies pasted at once");
    press(test::commandButton(w.child("shellHeader"), "history.undo"));
    require(w.session.validBodies().size() == 3, "Undo takes all three copies back");
}

void suppressSteps(Window& w) {
    w.click(*w.viewport, viewport::ViewportTestAccess::project(*w.viewport, -200, -200, 0),
            Qt::NoModifier); // nothing selected
    w.clickStep(0, Qt::ControlModifier);
    w.clickStep(1, Qt::ControlModifier);
    require(w.status() == QStringLiteral("2 Schritte gewählt"),
            "Two steps grouped: " + w.status().toStdString());
    w.capture("step-group");
    w.command("history.suppress").trigger();
    test::settleEvents();
    require(w.session.validBodies().size() == 1, "Both suppressed");
    press(test::commandButton(w.child("shellHeader"), "history.undo"));
    require(w.session.validBodies().size() == 3, "One undo brings both back");
}

void deleteBodies(Window& w) {
    w.clickBodyRow(0, Qt::NoModifier);
    w.clickBodyRow(1, Qt::ControlModifier);
    w.clickBodyRow(2, Qt::ControlModifier);
    w.command("history.delete").trigger();
    test::settleEvents();
    require(w.session.validBodies().empty(), "The three bodies deleted together");
    press(test::commandButton(w.child("shellHeader"), "history.undo"));
    require(w.session.validBodies().size() == 3, "One undo restores all three");
}

int run() {
    test::DialogAnswers user;
    user.answer = QMessageBox::Discard;
    Window w;
    test::activate(*w.main); // keys reach only the active window
    threeBoxes(w);
    moveTogether(w);
    copyThree(w);
    suppressSteps(w);
    deleteBodies(w);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS N02b groups through the shell: three bodies chosen with Strg/Umschalt, "
                     "moved together, copied at once, steps suppressed and bodies deleted together"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
