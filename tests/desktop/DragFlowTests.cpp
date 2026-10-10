#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QStatusBar>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// S14 (A-TXN-03) in the product shell with real mouse events: a sketch element dragged with
// 100 moves is one sketch undo step; a second drag ended with Escape changes nothing and adds
// no step; 100 redraws change neither history nor the modified flag. The same for the body
// manipulator: 100 moves are one document undo step; Escape ends a drag without a change.
namespace {
using namespace voelund;
using test::press;
using Points = std::vector<std::pair<double, double>>;
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

bool samePoints(const Points& a, const Points& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t index = 0; index < a.size(); ++index)
        if (std::abs(a[index].first - b[index].first) > 1e-9 ||
            std::abs(a[index].second - b[index].second) > 1e-9)
            return false;
    return true;
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
        viewport->setCameraView(viewport::CameraView::top);
        test::render(*viewport);
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    QToolButton& rail(const char* id) { return test::commandButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QAction& command(const char* id) {
        for (auto* action : main->findChildren<QAction*>())
            if (action->property("commandId").toString() == QLatin1String(id))
                return *action;
        throw std::runtime_error(std::string("Command ") + id);
    }
    QPoint at(double x, double y, double z = 0) {
        return viewport::ViewportTestAccess::project(*viewport, x, y, z);
    }
    void click(double x, double y, double z = 0) {
        const QPoint point = at(x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    // Press at `from`, 100 moves towards `to`; then release, or Escape and release.
    void drag(QPoint from, QPoint to, bool escape) {
        test::mouse(*viewport, QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 100; ++step) {
            const QPoint point = from + (to - from) * step / 100;
            test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
        }
        if (escape) {
            test::activate(*main);
            viewport->setFocus();
            QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(viewport, &key);
        }
        test::mouse(*viewport, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    Points points() { return viewport::ViewportTestAccess::sketchPoints(*viewport); }
    [[nodiscard]] std::pair<double, double> bodyCenter() const {
        Bnd_Box box;
        for (const auto& body : session.currentBodies())
            BRepBndLib::Add(body.shape, box);
        double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
        box.Get(x0, y0, z0, x1, y1, z1);
        return {(x0 + x1) / 2, (y0 + y1) / 2};
    }
};

void sketchDrag(Window& w) {
    w.click(30, 30);
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.line"));
    w.click(2, 3); // free: not bound to the origin
    w.click(12, 3);
    press(w.rail("sketch.tool.select"));
    const Points drawn = w.points();
    const auto revision = w.session.sceneStatus().document;

    // The press takes the nearest end point; along the (horizontal) line it may move.
    w.drag(w.at(7, 3), w.at(10, 3), false);
    const Points dragged = w.points();
    require(!samePoints(dragged, drawn), "100 drag moves move the line");
    w.command("sketch.undo_sketch").trigger();
    test::settleEvents();
    require(samePoints(w.points(), drawn), "One sketch undo reverts the whole drag");
    w.command("sketch.redo_sketch").trigger();
    test::settleEvents();
    require(samePoints(w.points(), dragged), "Redo repeats it");

    // A second drag ended with Escape: no change, no new step (undo reverts the first drag).
    const QPoint grip = w.at(11, 3);
    w.drag(grip, grip + QPoint(80, 40), true);
    require(samePoints(w.points(), dragged), "Escape during a drag keeps the geometry");
    w.command("sketch.undo_sketch").trigger();
    test::settleEvents();
    require(samePoints(w.points(), drawn), "Escape added no undo step");
    w.command("sketch.redo_sketch").trigger();
    test::settleEvents();

    // 100 redraws: no history, no document change.
    const bool canUndo = w.command("sketch.undo_sketch").isEnabled();
    for (int frame = 0; frame < 100; ++frame)
        test::render(*w.viewport);
    require(w.session.sceneStatus().document == revision && !w.session.isModified() &&
                w.command("sketch.undo_sketch").isEnabled() == canUndo &&
                samePoints(w.points(), dragged),
            "100 redraws change neither history nor the document");

    test::answerDiscardQuestion(QStringLiteral("Verwerfen"));
    auto* cancel = w.child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolCancelButton"));
    press(*cancel);
}

void bodyDrag(Window& w) {
    press(w.rail("solid.create.box"));
    w.viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*w.viewport);
    const auto start = w.bodyCenter();
    w.command("solid.modify.transform").trigger();
    test::render(*w.viewport);
    const auto handle = viewport::ViewportTestAccess::translationHandle(*w.viewport);
    require(handle.has_value(), "The move handle is shown");
    const QPoint destination =
        viewport::ViewportTestAccess::translationDestination(*w.viewport, *handle);
    w.drag(*handle, destination, false);
    const auto moved = w.bodyCenter();
    require(std::hypot(moved.first - start.first, moved.second - start.second) > 1e-3,
            "100 manipulator moves move the box");
    const auto sameCenter = [](std::pair<double, double> a, std::pair<double, double> b) {
        return std::hypot(a.first - b.first, a.second - b.second) < 1e-9;
    };
    require(w.session.undo().status() == application::CommandStatus::committed &&
                sameCenter(w.bodyCenter(), start) && w.session.features().size() == 1,
            "One document undo reverts the whole drag (the box stays)");
    require(w.session.redo().status() == application::CommandStatus::committed &&
                sameCenter(w.bodyCenter(), moved),
            "Redo repeats it");
    test::render(*w.viewport);

    const auto afterMove = w.session.sceneStatus().document;
    const auto handleAgain = viewport::ViewportTestAccess::translationHandle(*w.viewport);
    require(handleAgain.has_value(), "The move handle is shown again");
    const QPoint away =
        viewport::ViewportTestAccess::translationDestination(*w.viewport, *handleAgain);
    w.drag(*handleAgain, away, true);
    const auto kept = w.bodyCenter();
    require(w.session.sceneStatus().document == afterMove &&
                std::hypot(kept.first - moved.first, kept.second - moved.second) < 1e-9,
            "Escape during a body drag changes nothing and adds no step");
}

int run() {
    Window w;
    sketchDrag(w);
    bodyDrag(w);
    std::cout << "PASS S14 drags in the shell (A-TXN-03): sketch and body, one undo step, "
                 "Escape without change, redraws without history\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    voelund::desktop::configureProductApplication();
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
