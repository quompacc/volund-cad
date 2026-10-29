// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// L09: docs/tutorial-referenzplatte.md step by step, as a user follows it: the keys and fields
// it names (a key is checked on its command, then the command runs), values typed into the
// fields at the pointer and into the focused panel field. Each section ends at the volume the
// tutorial gives.
namespace {
using namespace voelund;
using test::press;
constexpr double pi = std::numbers::pi;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}
bool closeTo(double value, double expected) {
    return std::abs(value - expected) < 1e-6 * std::max(1.0, std::abs(expected));
}

struct Tutorial final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    Tutorial() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        main->show();
        viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        require(viewport != nullptr, "viewport");
        view(viewport::CameraView::top);
    }
    void view(viewport::CameraView camera) {
        test::render(*viewport);
        viewport->setCameraView(camera);
        test::render(*viewport);
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    QToolButton& rail(const char* id) { return test::railButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    // The key the tutorial names is the command's shortcut; then the command runs.
    void shortcut(const char* id, const char* key) {
        for (auto* action : main->findChildren<QAction*>())
            if (action->property("commandId").toString() == QLatin1String(id)) {
                require(action->shortcut() == QKeySequence(QString::fromLatin1(key)),
                        std::string(id) + " has the key " + key);
                require(action->isEnabled(), std::string(id) + " is enabled; status: " +
                                                 main->statusBar()->currentMessage().toStdString() +
                                                 ", steps " + std::to_string(steps()));
                action->trigger();
                test::settleEvents();
                test::render(*viewport);
                return;
            }
        throw std::runtime_error(std::string("Command missing: ") + id);
    }
    void click(double x, double y, double z) {
        test::render(*viewport);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void key(QWidget& target, int code, const QString& text = {}) {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier, text);
        QApplication::sendEvent(&target, &press);
        QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier, text);
        QApplication::sendEvent(&target, &release);
        test::settleEvents();
        test::render(*viewport);
    }
    // Typing at the pointer: digits, Tab between fields, Enter at the end.
    void typeAtPointer(const QString& keys) {
        test::activate(*main);
        for (const QChar character : keys) {
            if (character == QLatin1Char('\t'))
                key(*viewport, Qt::Key_Tab);
            else if (character == QLatin1Char('\n'))
                key(*viewport, Qt::Key_Return);
            else
                key(*viewport, Qt::Key_0 + character.digitValue(), QString(character));
        }
    }
    // The field beside the tool's arrow has the keyboard: the value typed, Enter.
    void typeIntoFocus(const QString& digits) {
        auto* focused = QApplication::focusWidget();
        require(focused != nullptr && focused->objectName() == QStringLiteral("toolValueField") &&
                    focused->isVisible(),
                "The value field at the arrow has the keyboard");
        for (const QChar character : digits)
            key(*QApplication::focusWidget(), Qt::Key_0 + character.digitValue(),
                QString(character));
        key(*QApplication::focusWidget(), Qt::Key_Return);
    }
    void set(const char* field, double value) {
        auto* input =
            child("activeToolPanel")
                .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(field));
        require(input != nullptr && input->isVisible(), std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    void choose(const char* field, const QString& text) {
        auto* box =
            child("activeToolPanel")
                .findChild<QComboBox*>(QStringLiteral("toolChoice_") + QLatin1String(field));
        require(box != nullptr && box->findText(text) >= 0, "Choice " + text.toStdString());
        box->setCurrentIndex(box->findText(text));
        test::settleEvents();
    }
    void ok() {
        auto* button =
            child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
        require(button != nullptr && button->isEnabled(), "OK enabled");
        press(*button);
    }
    [[nodiscard]] double volume() const {
        double total = 0;
        for (const auto& body : session.validBodies()) {
            GProp_GProps properties;
            BRepGProp::VolumeProperties(body.shape, properties);
            total += properties.Mass();
        }
        return total;
    }
    // The pictures of the tutorial, when VOELUND_TUTORIAL_DIR names a folder.
    void capture(const char* name) {
        const QString directory = qEnvironmentVariable("VOELUND_TUTORIAL_DIR");
        if (directory.isEmpty())
            return;
        require(QDir().mkpath(directory), "Picture folder");
        // The view cube follows the camera on a 40 ms timer.
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 120)
            QApplication::processEvents(QEventLoop::AllEvents, 20);
        test::render(*viewport);
        test::settleEvents();
        const QString file =
            directory + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png");
        require(main->grab().save(file), "Saved " + file.toStdString());
    }
    // Isometric and out by the mouse wheel like a user, so the whole plate is in the picture.
    void overview() {
        view(viewport::CameraView::isometric);
        const QPointF center(viewport->width() / 2.0, viewport->height() / 2.0);
        for (int notch = 0; notch < 3; ++notch) {
            QWheelEvent wheel(center, viewport->mapToGlobal(center), QPoint(), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(viewport, &wheel);
            test::render(*viewport);
        }
        test::settleEvents();
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
        test::settleEvents();
    }
    void undo() {
        for (auto* action : main->findChildren<QAction*>())
            if (action->property("commandId").toString() == QStringLiteral("history.undo")) {
                require(action->shortcut() == QKeySequence::Undo, "Undo is Ctrl+Z");
                action->trigger();
                test::settleEvents();
                return;
            }
        press(test::commandButton(child("shellHeader"), "history.undo"));
    }
    QToolButton& step(int index) {
        int seen = 0;
        for (auto* item : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (item->isVisible() && seen++ == index)
                return *item;
        throw std::runtime_error("Timeline step missing");
    }
    void stepMenu(int index, const QString& text) {
        bool picked = false;
        QTimer::singleShot(0, [&picked, text] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* menu = qobject_cast<QMenu*>(widget); menu && menu->isVisible()) {
                    for (auto* action : menu->actions())
                        if (action->text() == text && action->isEnabled()) {
                            action->trigger();
                            picked = true;
                        }
                    menu->close();
                }
        });
        auto& item = step(index);
        emit item.customContextMenuRequested(item.rect().center());
        test::settleEvents();
        require(picked, "Timeline menu entry " + text.toStdString());
    }
    [[nodiscard]] std::array<double, 6> bounds() const {
        Bnd_Box box;
        for (const auto& body : session.validBodies())
            BRepBndLib::AddOptimal(body.shape, box, false, false);
        std::array<double, 6> result{};
        box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
        return result;
    }
    [[nodiscard]] int steps() const {
        int count = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            count += step->isVisible() ? 1 : 0;
        return count;
    }
};

// Sections 1 to 4: plate, hole, pocket, rounded edge.
void build(Tutorial& t) {
    t.click(35, 35, 0);
    require(t.main->statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "1.2 XY");
    press(t.context("solid.create.sketch"));
    t.shortcut("sketch.tool.center_rectangle", "Shift+R");
    t.click(0, 0, 0);
    t.typeAtPointer(QStringLiteral("60\t40\n"));
    t.capture("tutorial-1-skizze");
    t.ok();
    t.click(20, -5, 0); // 1.8: in the area, right of the centre, off the diagonals
    t.shortcut("solid.create.extrude_profiles", "E");
    t.typeIntoFocus(QStringLiteral("10"));
    require(closeTo(t.volume(), 24000) && t.steps() == 2,
            "1: 24000 mm3, got " + std::to_string(t.volume()));

    t.click(15, 5, 10);
    t.shortcut("solid.modify.hole", "H");
    t.choose("place", QStringLiteral("An zwei Kanten"));
    t.click(-30, 0, 10); // the left top edge
    t.click(0, -20, 10); // the front top edge
    t.set("offset1", 42);
    t.set("offset2", 20);
    t.set("diameter", 8);
    t.choose("end", QStringLiteral("Durchgehend"));
    t.overview();
    t.capture("tutorial-2-bohrung");
    t.ok();
    require(closeTo(t.volume(), 24000 - 160 * pi), "2: through hole");

    t.click(-17, 5, 10);
    press(t.context("solid.create.sketch"));
    t.shortcut("sketch.tool.center_rectangle", "Shift+R");
    t.click(-17, 0, 10);
    t.typeAtPointer(QStringLiteral("14\t20\n"));
    t.ok();
    t.click(-12, 0, 10); // 3.5: right of its centre, off the diagonals
    t.shortcut("solid.create.extrude_profiles", "E");
    t.choose("operation", QStringLiteral("Abziehen"));
    t.set("distance", 4);
    t.overview();
    t.capture("tutorial-3-tasche");
    t.ok();
    require(closeTo(t.volume(), 22880 - 160 * pi) && t.steps() == 5,
            "3: pocket, got " + std::to_string(t.volume()));

    t.view(viewport::CameraView::front);
    t.click(0, -20, 5);
    t.shortcut("solid.modify.fillet", "F");
    t.click(-30, -20, 5);
    t.set("radius", 1);
    t.overview();
    t.capture("tutorial-4-verrundung");
    t.ok();
    t.overview();
    t.capture("tutorial-4-fertig");
    require(closeTo(t.volume(), 22880 - 160 * pi - 10 * (1 - pi / 4)) && t.steps() == 6,
            "4: rounded edge, six steps");
}

// Sections 5 to 8: save and open, edits through the timeline, suppress, STEP.
void change(Tutorial& t, const QString& folder, test::DialogAnswers& user) {
    const double rounded = 22880 - 160 * pi - 10 * (1 - pi / 4);
    user.file = folder + QStringLiteral("/Referenzplatte.volund");
    t.file("document.save_as");
    t.file("document.new");
    t.file("document.open");
    require(closeTo(t.volume(), rounded) && t.steps() == 6 && user.errors == 0, "5: reopened");

    press(t.step(1));
    t.set("distance", 12);
    t.ok();
    require(closeTo(t.volume(), 28800 - 192 * pi - 1120 - 12 * (1 - pi / 4)), "6: 12 high");
    t.undo();
    t.view(viewport::CameraView::top);
    press(t.step(0));
    press(t.rail("sketch.tool.select"));
    t.click(0, -20, 0);
    t.set("line_length", 70);
    t.ok();
    require(closeTo(t.volume(), rounded + 4000) && t.steps() == 6, "6: 70 wide");
    t.undo();
    press(t.step(5));
    t.set("radius", 2);
    t.ok();
    require(closeTo(t.volume(), 22880 - 160 * pi - 40 * (1 - pi / 4)), "6: radius 2");
    t.undo();
    require(closeTo(t.volume(), rounded), "6: undone");

    t.stepMenu(4, QStringLiteral("Unterdrücken"));
    require(closeTo(t.volume(), rounded + 1120), "7: pocket suppressed");
    t.stepMenu(4, QStringLiteral("Aktivieren"));
    require(closeTo(t.volume(), rounded), "7: pocket active");

    const auto box = t.bounds();
    user.file = folder + QStringLiteral("/Referenzplatte.step");
    t.file("document.export_step");
    user.answer = QMessageBox::Discard;
    t.file("document.new");
    t.file("document.import_step");
    const auto imported = t.bounds();
    bool same = true;
    for (std::size_t i = 0; i < box.size(); ++i)
        same = same && std::abs(box[i] - imported[i]) < 1e-5;
    require(closeTo(t.volume(), rounded) && same && t.steps() == 1 && user.errors == 0,
            "8: STEP keeps volume and size, one step Import 1");
}

} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "temporary directory");
        test::DialogAnswers user;
        Tutorial tutorial;
        test::activate(*tutorial.main);
        build(tutorial);
        change(tutorial, directory.path(), user);
        std::cout << "PASS L09 tutorial as written: plate, hole by two edges, pocket, rounded "
                     "edge, save/open, height, width, radius, suppress, STEP\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
