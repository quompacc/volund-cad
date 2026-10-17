#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// S14 (A-REG-01/02 through the product shell): a reference sketch on a body face, drawn and
// used only through the real UI. Box from the rail -> sketch on its top face (projected edges
// shown) -> two rectangles, a circle inside the second -> OK. The plain rectangle, the ring and
// the inner disc are three separate regions; the ring is extruded 5 mm (analytic volume).
// Save/Open through the file menu keeps it. Editing the circle's radius from the timeline keeps
// the ring as the extruded region: the volume follows the new ring area.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

constexpr double top = 40.0; // the default box: 20 x 20 x 40
constexpr double boxVolume = 20.0 * 20.0 * 40.0;
constexpr double outerArea = 7.0 * 16.0; // rectangle (11,2)-(18,18)
constexpr double pi = std::numbers::pi;

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
    QToolButton& rail(const char* id) { return test::railButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QPushButton& panelButton(const char* name) {
        auto* button = child("activeToolPanel").findChild<QPushButton*>(QLatin1String(name));
        require(button != nullptr, name);
        return *button;
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
    }
    void click(double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, top);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void type(const char* field, double value) {
        auto* input =
            child("activeToolPanel")
                .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(field));
        require(input != nullptr && input->isVisible(),
                std::string("Field ") + field +
                    ", status: " + main->findChild<QStatusBar*>()->currentMessage().toStdString());
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    QToolButton& timelineStep(int index) {
        int seen = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (step->isVisible() && seen++ == index)
                return *step;
        throw std::runtime_error("Timeline step missing");
    }
    // The region a click at (x, y) on the sketch selects.
    document::SketchRegionId region(double x, double y) {
        click(x, y);
        const auto selected = session.selectedSketchProfiles();
        require(selected.size() == 1,
                "One region at (" + std::to_string(x) + ", " + std::to_string(y) +
                    "), status: " + main->findChild<QStatusBar*>()->currentMessage().toStdString());
        return selected.front();
    }
    // With VOELUND_UI_FLOW_ARTIFACT_DIR: the window as the user sees it.
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
    void iso(const char* name) {
        viewport->setCameraView(viewport::CameraView::isometric);
        test::render(*viewport);
        capture(name);
        viewport->setCameraView(viewport::CameraView::top);
        test::render(*viewport);
    }
    [[nodiscard]] double volume() const {
        double total = 0.0;
        for (const auto& body : session.currentBodies()) {
            GProp_GProps properties;
            BRepGProp::VolumeProperties(body.shape, properties);
            total += properties.Mass();
        }
        return total;
    }
};

void requireVolume(const Window& w, double ringArea, const std::string& when) {
    const double expected = boxVolume + ringArea * 5.0;
    require(std::abs(w.volume() - expected) < 1.0e-3 * expected,
            when + ": volume " + std::to_string(w.volume()) + ", expected " +
                std::to_string(expected));
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    const QString path = directory.path() + QStringLiteral("/Referenzskizze.volund");
    test::DialogAnswers user;
    {
        Window w;
        press(w.rail("solid.create.box"));
        require(w.session.features().size() == 1, "Box from the rail");

        // Sketch on the top face: its edges are projected into the sketch.
        w.click(10, 10);
        press(w.context("solid.create.sketch"));
        require(viewport::ViewportTestAccess::projectedEdges(*w.viewport) > 0,
                "The face sketch shows the projected face edges");
        press(w.rail("sketch.tool.rectangle"));
        w.click(2, 2);
        w.click(9, 18);
        w.click(11, 2);
        w.click(18, 18);
        press(w.rail("sketch.tool.circle"));
        w.click(14.5, 10);
        w.click(16.5, 10);
        press(w.panelButton("toolAcceptButton"));
        require(w.session.features().size() == 2, "The reference sketch is stored");
        w.iso("s14-face-sketch"); // the sketch lines on the face stay visible

        // A-REG-01: rectangle, ring and inner disc are three regions; the ring is not filled.
        // The regions lie on the box face and win the click over it.
        // Picked in the view after another selection, the sketch is shown with its region.
        w.click(-30, -30);
        require(viewport::ViewportTestAccess::sketchPoints(*w.viewport).empty(),
                "Nothing selected: no sketch shown");
        const auto plain = w.region(5, 10);
        require(!viewport::ViewportTestAccess::sketchPoints(*w.viewport).empty(),
                "The picked region's sketch is shown");
        const auto ring = w.region(12, 4);
        const auto disc = w.region(14.5, 10);
        require(plain != ring && ring != disc && plain != disc, "Three separate regions");

        // The ring extruded 5 mm through the context bar and the panel.
        require(w.region(12, 4) == ring, "The ring again");
        press(w.context("solid.create.extrude_profiles"));
        w.type("distance", 5);
        press(w.panelButton("toolAcceptButton"));
        require(w.session.features().size() == 3, "Extrusion of the ring");
        requireVolume(w, outerArea - 4.0 * pi, "Ring extrusion");
        {
            // Hover over the box face beside the ring body: the highlight stays behind it.
            w.viewport->setCameraView(viewport::CameraView::isometric);
            test::render(*w.viewport);
            const QPoint corner = viewport::ViewportTestAccess::project(*w.viewport, 1, 1, top);
            test::mouse(*w.viewport, QEvent::MouseMove, corner, Qt::NoButton, Qt::NoButton);
            w.capture("s14-hover-face");
            w.viewport->setCameraView(viewport::CameraView::top);
            test::render(*w.viewport);
        }

        user.file = path;
        w.file("document.save_as");
        require(!w.session.isModified(), "Saved through the file menu");
        w.file("document.new");
        require(w.session.features().empty(), "New document");
        w.file("document.open");
        require(w.session.features().size() == 3, "Opened through the file menu");
        requireVolume(w, outerArea - 4.0 * pi, "After Save/Open");

        // A-REG-02: a radius change keeps the ring as the extruded region. The extrusion lies
        // over the sketch; in the editor only sketch geometry is picked.
        press(w.timelineStep(1));
        require(w.main->findChild<QWidget*>(QStringLiteral("activeToolPanel"))->isVisible(),
                "The sketch opens from the timeline");
        press(w.rail("sketch.tool.select"));
        w.click(14.5, 12); // the circle's top
        // The circle's center caught the grid at (14, 10): radius 3 touches the left side
        // x = 11, the ring breaks apart, and OK is refused with the elements named.
        w.type("circle_radius", 3);
        press(w.panelButton("toolAcceptButton"));
        const QString refusal = w.main->findChild<QLabel*>(QStringLiteral("toolGuidance"))->text();
        require(w.main->findChild<QWidget*>(QStringLiteral("activeToolPanel"))->isVisible() &&
                    refusal.contains(QStringLiteral("needs repair")) &&
                    refusal.contains(QStringLiteral("[elements 8 9]")),
                "A touching circle is refused with the elements named: " + refusal.toStdString());
        requireVolume(w, outerArea - 4.0 * pi, "Refused: nothing changed");
        w.type("circle_radius", 2.5);
        press(w.panelButton("toolAcceptButton"));
        requireVolume(w, outerArea - 6.25 * pi, "Radius 2.5: the ring stays extruded");
        user.answer = QMessageBox::Discard;
    }
    std::cout << "PASS S14 reference sketch through the shell: face sketch, three regions, "
                 "ring extrusion, Save/Open, radius change keeps the region\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
