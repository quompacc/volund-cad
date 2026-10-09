#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "ModelTreeLookup.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// U11 (A-UI-01 sketch part, A-VIS-01 captures): the whole product path through the real UI,
// run inside the started event loop. Buttons of context bar, rail, panel, timeline and the file
// menu are clicked with the mouse; fields are typed. New -> XY sketch -> rectangle 20 x 10 ->
// fixed relation -> OK -> edit from the timeline -> Cancel -> Undo/Redo -> Save -> Close ->
// Open, checking tree, timeline, document revision and rendering. A counter-check detaches
// one handler and shows that its button then does nothing, so a lost connection turns the
// flow red. With VOELUND_UI_FLOW_ARTIFACT_DIR set, the states of A-VIS-01 are captured in
// both themes and at the smallest window size.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    Window() {
        // U12: the product's composition root, without the recovery question.
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        main->show();
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
    QToolButton& header(const char* id) { return test::commandButton(child("shellHeader"), id); }
    QPushButton& panelButton(const char* name) {
        auto* button = child("activeToolPanel").findChild<QPushButton*>(QLatin1String(name));
        require(button != nullptr, name);
        return *button;
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
    }
    void click(double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, 0);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void addClick(double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, 0);
        for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent event(type, QPointF(point), QPointF(viewport->mapToGlobal(point)),
                              Qt::LeftButton,
                              type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                              Qt::ControlModifier);
            QApplication::sendEvent(viewport, &event);
        }
        test::render(*viewport);
        test::settleEvents();
    }
    void type(const char* field, double value) {
        auto* input = child("activeToolPanel")
                          .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") +
                                                      QLatin1String(field));
        require(input != nullptr && input->isVisible(), std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    [[nodiscard]] bool editing() {
        auto* panel = main->findChild<QWidget*>(QStringLiteral("activeToolPanel"));
        return panel != nullptr && panel->isVisible();
    }
    [[nodiscard]] int timelineSteps() {
        int steps = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            steps += step->isVisible() ? 1 : 0;
        return steps;
    }
    QToolButton& timelineStep(int index) {
        int seen = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (step->isVisible() && seen++ == index)
                return *step;
        throw std::runtime_error("Timeline step missing");
    }
    void capture(const char* name) {
        const QString directory = qEnvironmentVariable("VOELUND_UI_FLOW_ARTIFACT_DIR");
        if (directory.isEmpty())
            return;
        require(QDir().mkpath(directory), "capture directory");
        test::render(*viewport);
        require(main->grab().save(directory + QLatin1Char('/') + QLatin1String(name) +
                                  QStringLiteral(".png")),
                std::string("capture ") + name);
    }
};

// The stored rectangle: four sides, 20 x 10, its first corner locked at (2, 2).
void requireRectangle(const application::CadSession& session, const char* what) {
    const auto features = session.features();
    require(features.size() == 1 && features.front().kind == document::FeatureKind::sketch,
            std::string(what) + ": one sketch");
    const auto sketch = session.sketchParameters(features.front().id);
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto& element : sketch.elements.values) {
        const auto& line = std::get<document::SketchLine>(element.geometry);
        for (const auto p : {line.start, line.end}) {
            minX = std::min(minX, p.x), maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y), maxY = std::max(maxY, p.y);
        }
    }
    int locks = 0;
    for (const auto& constraint : sketch.constraints.values)
        locks += constraint.kind == document::SketchConstraintKind::coordinateX ||
                 constraint.kind == document::SketchConstraintKind::coordinateY;
    require(sketch.elements.values.size() == 4 && std::abs(maxX - minX - 20) < 1e-6 &&
                std::abs(maxY - minY - 10) < 1e-6 && locks == 2 && std::abs(minX - 2) < 1e-6 &&
                std::abs(minY - 2) < 1e-6,
            std::string(what) + ": rectangle 20 x 10 with its corner locked at (2, 2)");
}

void productPath(const QString& path) {
    test::DialogAnswers user;
    auto first = std::make_unique<Window>();
    Window& w = *first;
    const auto start = w.session.sceneStatus().document;

    w.file("document.new"); // clean: no question
    require(user.prompts == 0 && w.session.features().empty(), "New without a question");
    w.click(30, 30);
    require(w.main->statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "XY picked");
    press(w.context("solid.create.sketch"));
    require(w.editing(), "The context bar opens the sketch editor");

    // Away from the origin, so the corner is bound by the lock alone (nothing redundant).
    press(w.rail("sketch.tool.rectangle"));
    w.click(2, 2);
    w.click(20, 11);
    press(w.rail("sketch.tool.select"));
    w.click(2.2, 2.2); // the first corner, locked before the sides get their lengths
    press(w.rail("sketch.lock_point"));
    w.click(11, 2); // bottom side
    w.type("line_length", 20);
    w.click(2, 6.5); // left side
    w.type("line_length", 10);
    auto* status = w.child("activeToolPanel").findChild<QLabel*>(QStringLiteral("toolStatus"));
    require(status != nullptr && status->text().contains(QStringLiteral("Vollständig bestimmt")),
            "Dimensions and the lock determine the rectangle");
    w.capture("sketch-with-labels");
    press(w.panelButton("toolAcceptButton"));
    test::render(*w.viewport);
    requireRectangle(w.session, "After OK");
    require(!w.editing() && w.timelineSteps() == 1 && w.session.isModified() &&
                w.session.sceneStatus().document.revision > start.revision &&
                w.viewport->isRenderingReady(),
            "OK: editor closed, one timeline step, new revision, rendered");
    auto* tree = w.main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(tree != nullptr && test::featureRow(*tree, w.session, 0) != nullptr,
            "The sketch has its tree row");

    // Edit from the timeline, Cancel without changes.
    const auto accepted = w.session.sceneStatus().document;
    press(w.timelineStep(0));
    require(w.editing(), "The timeline step opens the editor");
    press(w.panelButton("toolCancelButton"));
    require(!w.editing() && w.session.sceneStatus().document == accepted,
            "Cancel from the timeline keeps the document");

    // Undo and Redo through the header.
    press(w.header("history.undo"));
    require(w.session.features().empty() && w.timelineSteps() == 0, "Undo removes the sketch");
    press(w.header("history.redo"));
    requireRectangle(w.session, "After Redo");

    // Save (first time: Save As), Close, Open in a new window.
    user.file = path;
    w.file("document.save");
    require(user.fileDialogs == 1 && !w.session.isModified() && user.errors == 0, "Saved");
    require(w.main->close(), "Close after saving asks nothing");
    first.reset();

    Window again;
    again.file("document.open");
    requireRectangle(again.session, "After Open");
    require(!again.session.isModified() && again.timelineSteps() == 1 &&
                again.viewport->isRenderingReady(),
            "Opened: unmodified, one timeline step, rendered");
    auto* reopenedTree = again.main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(reopenedTree != nullptr && test::featureRow(*reopenedTree, again.session, 0) != nullptr,
            "The reopened sketch has its tree row");

    // Counter-check: a button whose handler is detached does nothing.
    again.click(60, 60); // nothing selected
    press(again.rail("solid.create.box"));
    require(again.session.features().size() == 2, "The box button creates a box");
    press(again.header("history.undo"));
    QAction* box = again.rail("solid.create.box").defaultAction();
    QObject::disconnect(box, &QAction::triggered, nullptr, nullptr);
    press(again.rail("solid.create.box"));
    require(again.session.features().size() == 1,
            "With its handler detached the box button does nothing");
    user.answer = QMessageBox::Discard;
    require(again.main->close(), "Close");
}

// A-VIS-01: selected part, sketch with labels, extrusion panel with preview, an error, the
// state after Cancel; light and dark, normal and smallest size.
void captures(const QString& path) {
    if (qEnvironmentVariable("VOELUND_UI_FLOW_ARTIFACT_DIR").isEmpty())
        return;
    test::DialogAnswers user;
    user.file = path;
    user.answer = QMessageBox::Discard;
    Window w;
    w.file("document.open");
    for (const char* theme : {"light", "dark"}) {
        for (const bool small : {false, true}) {
            if (small)
                w.main->resize(w.main->minimumSizeHint());
            else
                w.main->resize(1300, 860);
            test::render(*w.viewport);
            const std::string prefix = std::string(theme) + (small ? "-small-" : "-");
            w.viewport->setCameraView(viewport::CameraView::top);
            press(w.timelineStep(0));
            w.capture((prefix + "sketch-labels").c_str());
            // An error: a fillet radius larger than the sides allow (reason, red sides).
            press(w.rail("sketch.tool.select"));
            w.click(12, 2);
            w.addClick(2, 7);
            w.type("fillet_radius", 50);
            press(w.rail("sketch.fillet"));
            w.capture((prefix + "error").c_str());
            test::answerDiscardQuestion(QStringLiteral("Verwerfen"));
            press(w.panelButton("toolCancelButton"));
            w.click(12, 7); // the rectangle's area
            press(w.context("solid.create.extrude_profiles"));
            w.viewport->setCameraView(viewport::CameraView::isometric);
            w.capture((prefix + "extrude-preview").c_str());
            press(w.panelButton("toolCancelButton"));
            w.capture((prefix + "after-cancel").c_str());
            w.viewport->setCameraView(viewport::CameraView::top);
            w.click(12, 7);
            press(w.context("solid.create.extrude_profiles"));
            press(w.panelButton("toolAcceptButton"));
            w.click(12, 7);
            w.viewport->setCameraView(viewport::CameraView::isometric);
            w.capture((prefix + "selected-part").c_str());
            press(w.header("history.undo"));
        }
        w.main->resize(1300, 860);
        if (std::string(theme) == "light")
            for (auto* action : w.main->findChildren<QAction*>())
                if (action->property("commandId").toString() == QStringLiteral("view.theme")) {
                    action->trigger();
                    break;
                }
    }
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    const QString path = directory.path() + QStringLiteral("/U11 Ablauf.volund");
    productPath(path);
    captures(path);
    std::cout << "PASS U11 product path through the real UI (A-UI-01 sketch part)\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    voelund::desktop::configureProductApplication();
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    // U11: the flow runs inside the started event loop, as a user's clicks would.
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
