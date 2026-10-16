#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <QWheelEvent>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// README pictures (not a test): the reference plate of A-PART-01 built through the product
// shell with clicks and fields, and a ring beside it. Captures the sketch, the hole and fillet
// previews, the finished model in light and dark, measuring and an explained error into
// VOELUND_SHOWCASE_DIR (docs/images in the repository). Run on a desktop session or the virtual
// Wayland of the desktop tests.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    QString directory;
    Window() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1500, 940);
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
    // Isometric, the whole model, then out by `notches` of the mouse wheel like a user, so the
    // cards around the view leave it free.
    void overview(int notches = 4) {
        view(viewport::CameraView::isometric);
        const QPointF center(viewport->width() / 2.0, viewport->height() / 2.0);
        for (int i = 0; i < notches; ++i) {
            QWheelEvent wheel(center, viewport->mapToGlobal(center), QPoint(), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(viewport, &wheel);
            test::render(*viewport);
        }
        test::settleEvents();
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    QToolButton& rail(const char* id) { return test::railButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QToolButton& header(const char* id) { return test::commandButton(child("shellHeader"), id); }
    QPushButton& panelButton(const char* name) {
        auto* button = child("activeToolPanel").findChild<QPushButton*>(QLatin1String(name));
        require(button != nullptr, name);
        return *button;
    }
    void ok() { press(panelButton("toolAcceptButton")); }
    void cancel() { press(panelButton("toolCancelButton")); }
    void click(double x, double y, double z) {
        test::render(*viewport);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
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
    void choose(const char* field, int index) {
        auto* box = child("activeToolPanel")
                        .findChild<QComboBox*>(QStringLiteral("toolChoice_") + QLatin1String(field));
        require(box != nullptr && index < box->count(), std::string("Choice ") + field);
        box->setCurrentIndex(index);
        test::settleEvents();
    }
    QToolButton& step(int index) {
        int seen = 0;
        for (auto* item : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (item->isVisible() && seen++ == index)
                return *item;
        throw std::runtime_error("Timeline step missing");
    }
    void capture(const char* name) {
        for (int i = 0; i < 3; ++i) {
            test::render(*viewport);
            test::settleEvents();
        }
        const QString file = directory + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png");
        require(main->grab().save(file), "Saved " + file.toStdString());
        std::cout << "captured " << file.toStdString() << std::endl;
    }
};

int run() {
    const QString directory = qEnvironmentVariable("VOELUND_SHOWCASE_DIR");
    require(!directory.isEmpty() && QDir().mkpath(directory), "VOELUND_SHOWCASE_DIR");
    Window w;
    w.directory = directory;
    test::activate(*w.main);

    // Sketch: the plate's rectangle on XY.
    w.click(35, 35, 0);
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.rectangle"));
    w.click(-30, -20, 0);
    w.click(30, 20, 0);
    w.capture("03-skizze");
    w.ok();
    w.click(0, 0, 0);
    press(w.context("solid.create.extrude_profiles"));
    w.type("distance", 10);
    w.ok();

    // Hole: through, 8 mm, previewed.
    w.click(12, 0, 10);
    press(w.rail("solid.modify.hole"));
    w.type("diameter", 8);
    w.choose("end", 1);
    w.overview();
    w.capture("04-bohrung");
    w.ok();

    // Pocket from a sketch on the top face.
    w.view(viewport::CameraView::top);
    w.click(-17, 0, 10);
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.rectangle"));
    w.click(-24, -10, 10);
    w.click(-10, 10, 10);
    w.ok();
    w.click(-17, 0, 10);
    press(w.context("solid.create.extrude_profiles"));
    w.choose("operation", 2);
    w.type("distance", 4);
    w.ok();

    // Fillet on the front vertical edge at (30, -20), previewed.
    w.view(viewport::CameraView::front);
    w.click(0, -20, 5);
    press(w.rail("solid.modify.fillet"));
    w.click(30, -20, 5);
    w.type("radius", 4);
    w.overview();
    w.capture("05-verrundung");
    w.ok();

    // A ring of its own beside the plate.
    document::SketchParameters ring{document::datumSketchPlane(document::DatumPlane::xy), {}};
    ring.datum = document::DatumPlane::xy;
    ring.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{75, 0}, 20}});
    ring.elements.values.push_back(
        {document::SketchElementId{2}, document::SketchCircle{{75, 0}, 10}});
    ring.elements.nextId = 3;
    require(w.session.createDatumSketchExtrusion(ring, {}, 8).status() ==
                application::CommandStatus::committed,
            "Ring");
    w.session.clearSelection();
    w.main->refreshAfterExternalChange();

    // The finished model, light and dark.
    w.overview();
    w.capture("01-modell-hell");
    press(w.header("view.theme"));
    w.overview();
    w.capture("02-modell-dunkel");
    press(w.header("view.theme"));

    // Measuring: the top face and the front face of the plate.
    w.overview();
    press(w.rail("solid.inspect.measure"));
    w.click(20, 10, 10);
    w.click(20, -20, 5);
    w.capture("06-messen");
    w.cancel();

    // An explained error: the fillet reopened from the timeline with a radius too large.
    press(w.step(5));
    w.type("radius", 45);
    w.overview();
    w.capture("07-fehler-erklaert");
    w.cancel();
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        return run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
