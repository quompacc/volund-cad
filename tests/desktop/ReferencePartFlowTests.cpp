#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// M18 (A-PART-01 through the product shell, with A-HISTORY and A-STEP-01 parts): only clicks,
// fields and menus. An XY rectangle (-30,-20)..(30,20) extruded 10 (24000); a through hole of 8
// clicked onto the top face near (12, 0) (24000 - 160 pi); a pocket (-24,-10)..(-10,10) 4 deep
// from a sketch on the top face (22880 - 160 pi); the vertical edge at (-30,-20) rounded by 1
// (22880 - 160 pi - 10 (1 - pi/4)). Save, New, Open keep it. The extrusion reopened from the
// timeline to 12 carries hole, pocket and fillet; Undo/Redo in the header. The pocket
// suppressed and reactivated from the timeline menu. File > Export and, in a new document,
// File > Import give the same volume and bounding box.
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
    QToolButton& header(const char* id) { return test::commandButton(child("shellHeader"), id); }
    void ok() {
        auto* button =
            child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
        require(button != nullptr && button->isEnabled(), "OK enabled");
        press(*button);
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
        test::settleEvents();
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
    void type(const char* field, double value) {
        auto* input =
            child("activeToolPanel")
                .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(field));
        require(input != nullptr && input->isVisible(), std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    void choose(const char* field, int index) {
        auto* box =
            child("activeToolPanel")
                .findChild<QComboBox*>(QStringLiteral("toolChoice_") + QLatin1String(field));
        require(box != nullptr && box->isVisible() && index < box->count(),
                std::string("Choice ") + field);
        box->setCurrentIndex(index);
        test::settleEvents();
    }
    [[nodiscard]] int steps() const {
        int count = 0;
        for (auto* step : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            count += step->isVisible() ? 1 : 0;
        return count;
    }
    QToolButton& step(int index) {
        int seen = 0;
        for (auto* item : main->findChildren<QToolButton*>(QStringLiteral("timelineStep")))
            if (item->isVisible() && seen++ == index)
                return *item;
        throw std::runtime_error("Timeline step missing");
    }
    // Picks `text` from the right-click menu of timeline step `index`.
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
    [[nodiscard]] double volume() const {
        double total = 0;
        for (const auto& body : session.validBodies()) {
            GProp_GProps properties;
            BRepGProp::VolumeProperties(body.shape, properties);
            total += properties.Mass();
        }
        return total;
    }
    [[nodiscard]] std::array<double, 6> bounds() const {
        Bnd_Box box;
        for (const auto& body : session.validBodies())
            BRepBndLib::AddOptimal(body.shape, box, false, false);
        std::array<double, 6> result{};
        box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
        return result;
    }
};

bool closeTo(double value, double expected) {
    return std::abs(value - expected) < 1e-6 * std::max(1.0, std::abs(expected));
}

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    test::DialogAnswers user;
    Window w;
    test::activate(*w.main);
    const double plate = 24000;
    const double drilled = plate - 160 * pi;
    const double pocketed = drilled - 1120;
    const double rounded = pocketed - 10 * (1 - pi / 4);

    // 1. Base plate.
    w.click(35, 35, 0);
    require(w.main->statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "XY picked");
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.rectangle"));
    w.click(-30, -20, 0);
    w.click(30, 20, 0);
    w.ok();
    w.click(0, 0, 0);
    press(w.context("solid.create.extrude_profiles"));
    w.type("distance", 10);
    w.ok();
    require(closeTo(w.volume(), plate) && w.steps() == 2,
            "Plate 60 x 40 x 10: " + std::to_string(w.volume()));

    // 2. Through hole of 8 on the top face near (12, 0).
    w.click(12, 0, 10);
    press(w.rail("solid.modify.hole"));
    w.type("diameter", 8);
    w.choose("end", 1);
    w.ok();
    require(closeTo(w.volume(), drilled) && w.steps() == 3,
            "Through hole: " + std::to_string(w.volume()));

    // 3. Pocket from a sketch on the top face.
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
    require(closeTo(w.volume(), pocketed) && w.steps() == 5,
            "Pocket: " + std::to_string(w.volume()));

    // 4. The vertical edge at (-30, -20), radius 1.
    w.view(viewport::CameraView::front);
    w.click(0, -20, 5);
    press(w.rail("solid.modify.fillet"));
    w.click(-30, -20, 5);
    w.type("radius", 1);
    w.ok();
    require(closeTo(w.volume(), rounded) && w.steps() == 6,
            "Rounded edge: " + std::to_string(w.volume()));

    // 5. Save, New, Open.
    user.file = directory.path() + QStringLiteral("/Referenzplatte.volund");
    w.file("document.save_as");
    w.file("document.new");
    require(w.session.features().empty(), "New");
    w.file("document.open");
    require(closeTo(w.volume(), rounded) && w.steps() == 6 && user.errors == 0,
            "Opened with all six steps");

    // 6. The extrusion reopened from the timeline: 12 high carries the whole chain.
    const double twelve = 28800 - 192 * pi - 1120 - 12 * (1 - pi / 4);
    press(w.step(1));
    w.type("distance", 12);
    w.ok();
    require(closeTo(w.volume(), twelve),
            "Height 12 through the chain: " + std::to_string(w.volume()));
    press(w.header("history.undo"));
    require(closeTo(w.volume(), rounded), "Undo");
    press(w.header("history.redo"));
    require(closeTo(w.volume(), twelve), "Redo");
    press(w.header("history.undo"));

    // 7. The pocket suppressed and reactivated from the timeline menu.
    w.stepMenu(4, QStringLiteral("Unterdrücken"));
    require(closeTo(w.volume(), rounded + 1120), "Pocket suppressed");
    w.stepMenu(4, QStringLiteral("Aktivieren"));
    require(closeTo(w.volume(), rounded), "Pocket active again");

    // 8. STEP out and back in.
    const auto box = w.bounds();
    user.file = directory.path() + QStringLiteral("/Referenzplatte.step");
    w.file("document.export_step");
    user.answer = QMessageBox::Discard;
    w.file("document.new");
    w.file("document.import_step");
    const auto imported = w.bounds();
    bool sameBox = true;
    for (std::size_t i = 0; i < box.size(); ++i)
        sameBox = sameBox && std::abs(box[i] - imported[i]) < 1e-5;
    require(w.session.validBodies().size() == 1 && closeTo(w.volume(), rounded) && sameBox &&
                w.steps() == 1 && user.errors == 0,
            "STEP round trip keeps volume and bounding box");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS M18 A-PART-01 through the shell: plate, through hole, pocket, rounded "
                     "edge, save/new/open, height 12 through the chain, undo/redo, pocket "
                     "suppressed and reactivated, STEP round trip"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
