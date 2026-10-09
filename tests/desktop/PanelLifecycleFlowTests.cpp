#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPushButton>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U11 (A-UI-02): every tool panel of the product (sketch, extrusion, push/pull) through its real
// buttons: start, preview, change a value, Cancel or Escape -> the starting state exactly
// (revision, modified flag, undo, features); start again and OK -> exactly one undo step. An
// open sketch blocks the extrusion command, so no second editor starts.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

struct Snapshot final {
    application::DocumentStamp stamp;
    bool modified;
    bool canUndo;
    std::size_t features;
    bool operator==(const Snapshot&) const = default;
};
Snapshot snapshot(const application::CadSession& session) {
    return {session.sceneStatus().document, session.isModified(), session.canUndo(),
            session.features().size()};
}

struct Fixture final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> window;
    viewport::CadViewport* viewport{nullptr};
    Fixture() {
        session.newDocument();
        window = std::make_unique<desktop::MainWindow>(session);
        window->resize(1300, 860);
        window->show();
        viewport = dynamic_cast<viewport::CadViewport*>(window->centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
        viewport->setCameraView(viewport::CameraView::top);
        test::render(*viewport);
    }
    QWidget& named(const char* name) {
        auto* widget = window->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr, name);
        return *widget;
    }
    bool panelOpen() { return named("activeToolPanel").isVisible(); }
    QPushButton& panelButton(const char* name) {
        auto* button = named("activeToolPanel").findChild<QPushButton*>(QLatin1String(name));
        require(button != nullptr && button->isVisible(), name);
        return *button;
    }
    QToolButton& rail(const char* id) { return test::commandButton(named("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(named("contextBar"), id); }
    QToolButton& header(const char* id) { return test::commandButton(named("shellHeader"), id); }
    void click(double x, double y, double z = 0) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void type(const char* field, double value) {
        QDoubleSpinBox* input = nullptr;
        for (auto* candidate : named("activeToolPanel").findChildren<QDoubleSpinBox*>(
                 QStringLiteral("toolLength_") + QLatin1String(field)))
            if (candidate->isVisible())
                input = candidate;
        require(input != nullptr, std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    void escape() {
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(viewport, &key);
        test::settleEvents();
    }
};

void sketch(Fixture& f) {
    const auto start = snapshot(f.session);
    f.click(30, 30);
    press(f.context("solid.create.sketch"));
    press(f.rail("sketch.tool.line"));
    f.click(0, 0);
    f.click(10, 0);
    QAction* extrude = nullptr;
    for (auto* action : f.window->findChildren<QAction*>())
        if (action->property("commandId").toString() ==
            QStringLiteral("solid.create.extrude_profiles"))
            extrude = action;
    require(extrude != nullptr && !extrude->isEnabled(),
            "An open sketch blocks the extrusion: no second editor");
    test::answerDiscardQuestion(QStringLiteral("Verwerfen"));
    press(f.panelButton("toolCancelButton"));
    require(!f.panelOpen() && snapshot(f.session) == start, "Sketch Cancel: starting state");

    f.click(30, 30);
    press(f.context("solid.create.sketch"));
    press(f.rail("sketch.tool.rectangle"));
    f.click(2, 2);
    f.click(22, 12);
    press(f.panelButton("toolAcceptButton"));
    require(!f.panelOpen() && f.session.features().size() == 1, "Sketch OK");
    press(f.header("history.undo"));
    require(snapshot(f.session).features == start.features && !f.session.canUndo(),
            "Sketch OK is exactly one undo step");
    press(f.header("history.redo"));
}

void extrusion(Fixture& f) {
    const auto start = snapshot(f.session);
    f.click(12, 7); // the rectangle's area
    press(f.context("solid.create.extrude_profiles"));
    require(f.panelOpen() && f.panelButton("toolAcceptButton").isEnabled(),
            "Extrusion opens with a valid preview");
    f.type("distance", 12);
    press(f.panelButton("toolCancelButton"));
    require(!f.panelOpen() && snapshot(f.session) == start, "Extrusion Cancel: starting state");

    f.click(12, 7);
    press(f.context("solid.create.extrude_profiles"));
    f.type("distance", 8);
    f.escape();
    require(!f.panelOpen() && snapshot(f.session) == start, "Extrusion Escape: starting state");

    f.click(12, 7);
    press(f.context("solid.create.extrude_profiles"));
    f.type("distance", 8);
    press(f.panelButton("toolAcceptButton"));
    require(f.session.features().size() == start.features + 1, "Extrusion OK");
    press(f.header("history.undo"));
    require(snapshot(f.session).features == start.features, "Extrusion OK is one undo step");
    press(f.header("history.redo"));
}

void pushPull(Fixture& f) {
    f.viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*f.viewport);
    const auto start = snapshot(f.session);
    const auto pick = [&] {
        f.click(12, 7, 8); // the extrusion's top face
        press(f.context("solid.modify.push_pull"));
        require(f.panelOpen() && f.panelButton("toolAcceptButton").isEnabled(),
                "Push/pull opens with a valid preview");
    };
    pick();
    f.type("distance", 4);
    press(f.panelButton("toolCancelButton"));
    require(!f.panelOpen() && snapshot(f.session) == start, "Push/pull Cancel: starting state");
    pick();
    f.type("distance", 4);
    f.escape();
    require(!f.panelOpen() && snapshot(f.session) == start, "Push/pull Escape: starting state");
    pick();
    f.type("distance", 4);
    press(f.panelButton("toolAcceptButton"));
    require(f.session.features().size() == start.features + 1, "Push/pull OK");
    press(f.header("history.undo"));
    require(f.session.features().size() == start.features, "Push/pull OK is one undo step");
}

int run() {
    Fixture f;
    sketch(f);
    extrusion(f);
    pushPull(f);
    std::cout << "PASS U11 panel lifecycle (A-UI-02): sketch, extrusion, push/pull\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    int result = 1;
    QTimer::singleShot(0, [&result] {
        try {
            result = run();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        QApplication::exit(result);
    });
    application.exec();
    return result;
}
