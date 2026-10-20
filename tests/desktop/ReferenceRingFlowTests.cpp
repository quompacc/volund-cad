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
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// L08 (A-PART-02 through the product shell; until then only application.reference_ring):
// two concentric circles R20/r10 on XY, the ring region clicked and extruded 8 (2400 pi); a
// box from the rail set to 10 x 10 x 10 at X 60 in the properties card. The outer radius 24 in
// the reopened sketch gives 3808 pi, the box stays 1000. Outer radius 10 lets both circles
// coincide: OK is refused with the region named, the sketch is kept with the error at the
// extrusion, which fails, while the box stays valid and no filled disc appears. The error
// state is saved under a path with umlauts, opened again and repaired with radius 20.
namespace {
using namespace voelund;
using test::press;
constexpr double pi = std::numbers::pi;
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
    QToolButton& rail(const char* id) { return test::railButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QPushButton& panelButton(const char* name) {
        auto* button = child("activeToolPanel").findChild<QPushButton*>(QLatin1String(name));
        require(button != nullptr && button->isVisible(), std::string("Panel button ") + name);
        return *button;
    }
    void ok() {
        auto& button = panelButton("toolAcceptButton");
        require(button.isEnabled(), "OK enabled");
        press(button);
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
        test::settleEvents();
    }
    void click(double x, double y, double z = 0) {
        test::render(*viewport);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
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
        require(input != nullptr && input->isVisible(), std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    // A value typed into the properties card and confirmed, as a user does.
    void property(const char* name, double value) {
        auto* input = main->findChild<QDoubleSpinBox*>(QLatin1String(name));
        require(input != nullptr && input->isVisible(), std::string("Property ") + name);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    QToolButton& step(int index) {
        int seen = 0;
        for (auto* item : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (item->isVisible() && seen++ == index)
                return *item;
        throw std::runtime_error("Timeline step missing");
    }
    // Reopens the ring sketch from the timeline and gives the circle through (x, y) `radius`.
    void radius(double x, double y, double value) {
        press(step(0));
        require(child("activeToolPanel").isVisible(), "The sketch opens from the timeline");
        press(rail("sketch.tool.select"));
        click(x, y);
        type("circle_radius", value);
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
    [[nodiscard]] bool ringFails() const {
        for (const auto& state : session.featureStates())
            if (state.status != application::FeatureRuntimeStatus::current)
                return true;
        return false;
    }
};

bool closeTo(double value, double expected) {
    return std::abs(value - expected) < 1e-6 * std::max(1.0, std::abs(expected));
}

constexpr double ring = 2400 * pi;

void ringAndBox(Window& w) {
    // The ring: two concentric circles, the region between them clicked and extruded 8.
    w.click(35, 35);
    require(w.main->statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "XY picked");
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.circle"));
    w.click(0, 0);
    w.click(20, 0);
    press(w.rail("sketch.tool.circle"));
    w.click(0, 0);
    w.click(10, 0);
    w.ok();
    w.click(0, 15);
    press(w.context("solid.create.extrude_profiles"));
    w.type("distance", 8);
    w.ok();
    require(closeTo(w.volume(), 2400 * pi) && w.session.validBodies().size() == 1,
            "Ring R20/r10 x 8: " + std::to_string(w.volume()));

    // An independent box 10 x 10 x 10 well apart in X.
    w.click(-60, 60);
    press(w.rail("solid.create.box"));
    w.property("boxLengthInput", 10);
    w.property("boxWidthInput", 10);
    w.property("boxHeightInput", 10);
    w.property("boxTranslation0Input", 60);
    require(w.session.validBodies().size() == 2 && closeTo(w.volume(), ring + 1000),
            "Ring and box 10^3 as two bodies: " + std::to_string(w.volume()));
}

void breakRing(Window& w) {
    // Outer radius 24: 3808 pi, the box unchanged.
    w.radius(0, 20, 24);
    w.ok();
    require(w.session.validBodies().size() == 2 && closeTo(w.volume(), 3808 * pi + 1000),
            "Outer radius 24: " + std::to_string(w.volume()));

    // Outer radius 10: both circles coincide. Kept with the error at the extrusion: the ring
    // fails, the box stays, no filled disc.
    w.radius(0, 24, 10);
    // OK refuses and names the extrusion's region; keeping the change is offered.
    press(w.panelButton("toolAcceptButton"));
    const QString refusal = w.main->findChild<QLabel*>(QStringLiteral("toolGuidance"))->text();
    require(refusal.contains(QStringLiteral("Bereich")) && closeTo(w.volume(), 3808 * pi + 1000),
            "Refused with the region named: " + refusal.toStdString());
    press(w.panelButton("keepSketchErrorsButton"));
    require(w.ringFails() && w.session.validBodies().size() == 1 && closeTo(w.volume(), 1000),
            "Coinciding radii: ring failed, box valid: " + std::to_string(w.volume()));
}

void saveAndRepair(Window& w, test::DialogAnswers& user, const QTemporaryDir& directory) {
    // The error state saved under a path with umlauts and opened again.
    require(QDir(directory.path()).mkdir(QStringLiteral("Prüf Ordner äöü")), "Unicode folder");
    user.file = directory.path() + QStringLiteral("/Prüf Ordner äöü/Ring mit Fehler Ä.volund");
    w.file("document.save_as");
    w.file("document.new");
    require(w.session.features().empty(), "New");
    w.file("document.open");
    require(w.ringFails() && w.session.validBodies().size() == 1 && closeTo(w.volume(), 1000) &&
                user.errors == 0,
            "Opened with the failed ring and the valid box");

    // Repaired: the circle at radius 10 back to 20.
    w.radius(0, 10, 20);
    w.ok();
    require(!w.ringFails() && w.session.validBodies().size() == 2 &&
                closeTo(w.volume(), ring + 1000),
            "Repaired with radius 20: " + std::to_string(w.volume()));
    user.answer = QMessageBox::Discard;
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    test::DialogAnswers user;
    Window w;
    test::activate(*w.main);
    ringAndBox(w);
    breakRing(w);
    saveAndRepair(w, user, directory);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    // The file dialogs are answered as widgets (test::DialogAnswers), also on Windows.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS L08 A-PART-02 through the shell: ring 2400 pi, box 10^3, outer radius "
                     "24, coinciding radii fail only the ring, error saved and opened under an "
                     "umlaut path, repaired"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
