#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// M01 (A-BODY-01, A-BODY-03) through the product shell, from an empty document without a
// starter body or component: body A from an XY sketch, body B from an XZ sketch, body C a
// cylinder with nothing selected. The tree names them A/B/C, hides and shows them (stored,
// no undo step). With B selected, a new XY sketch joins A, chosen by name in the panel: A
// grows, B and C stay. Undo/Redo, Save/New/Open keep bodies, names, visibility and IDs.
namespace {
using namespace voelund;
using test::press;
using Kind = int; // ModelTreePanel::Kind values: 0 origin, 1 plane, 2 grid, 3 body
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    QTreeWidget* tree{nullptr};
    Window() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        tree = main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
        require(viewport != nullptr && tree != nullptr, "viewport and tree");
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
    void ok() {
        auto* button = child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
        require(button != nullptr, "OK");
        press(*button);
    }
    void file(const char* id) {
        test::chooseFromMenu(*main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")), id);
    }
    void click(double x, double y, double z) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void type(const char* field, double value) {
        auto* input = child("activeToolPanel").findChild<QDoubleSpinBox*>(
            QStringLiteral("toolLength_") + QLatin1String(field));
        require(input != nullptr && input->isVisible(), std::string("Field ") + field);
        input->setValue(value);
        emit input->editingFinished();
        test::settleEvents();
    }
    void choose(const char* field, const QString& text) {
        auto* box = child("activeToolPanel").findChild<QComboBox*>(QStringLiteral("toolChoice_") +
                                                                   QLatin1String(field));
        require(box != nullptr && box->isVisible() && box->findText(text) >= 0,
                std::string("Choice ") + field + " " + text.toStdString());
        box->setCurrentIndex(box->findText(text));
        test::settleEvents();
    }
    QTreeWidgetItem* row(Kind kind, const QString& text) {
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole + 2).toInt() == kind && (*it)->text(0) == text)
                return *it;
        throw std::runtime_error("Tree row " + text.toStdString());
    }
    void plane(const char* name) {
        emit tree->itemClicked(row(1, QString::fromUtf8(name)), 0);
        test::settleEvents();
    }
    void selectBody(const QString& name) {
        tree->setCurrentItem(row(3, name));
        test::settleEvents();
    }
    void rename(const QString& from, const QString& to) {
        row(3, from)->setText(0, to); // as the in-place editor commits it
        test::settleEvents();
    }
    // A rectangle on the open sketch's plane, then OK; then its region extruded `distance`.
    void rectangle(double x0, double y0, double z0, double x1, double y1, double z1) {
        press(rail("sketch.tool.rectangle"));
        click(x0, y0, z0);
        click(x1, y1, z1);
        ok();
    }
    [[nodiscard]] std::map<std::string, double> volumes() const {
        std::map<std::string, double> result;
        const auto outline = session.modelOutline();
        for (const auto& body : session.validBodies()) {
            std::string name;
            for (const auto& item : outline.bodies)
                if (item.id == body.bodyId)
                    name = item.name;
            GProp_GProps properties;
            BRepGProp::VolumeProperties(body.shape, properties);
            result[name] = properties.Mass();
        }
        return result;
    }
    [[nodiscard]] bool hidden(const std::string& name) const {
        for (const auto& body : session.modelOutline().bodies)
            if (body.name == name)
                return body.hidden;
        throw std::runtime_error("Body " + name);
    }
};

bool same(double a, double b) { return std::abs(a - b) < 1e-3 * std::max(1.0, std::abs(b)); }

int run() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    test::DialogAnswers user;
    user.file = directory.path() + QStringLiteral("/Mehrere Koerper.volund");
    Window w;
    require(w.session.features().empty(), "An empty document, no starter body");

    // A: XY sketch, region extruded 10 mm as a new body (20 x 10 x 10 = 2000).
    w.plane("XY-Ebene");
    press(w.context("solid.create.sketch"));
    w.rectangle(30, 0, 0, 50, 10, 0);
    w.view(viewport::CameraView::top);
    w.click(40, 5, 0);
    press(w.context("solid.create.extrude_profiles"));
    w.type("distance", 10);
    w.ok();
    // B: XZ sketch away from A, extruded 10 mm (20 x 10 x 10).
    w.plane("XZ-Ebene");
    press(w.context("solid.create.sketch"));
    w.rectangle(-60, 0, 5, -40, 0, 15);
    w.view(viewport::CameraView::front);
    w.click(-50, 0, 10);
    press(w.context("solid.create.extrude_profiles"));
    w.type("distance", 10);
    w.ok();
    // C: a cylinder with nothing selected is a new body at the origin.
    w.view(viewport::CameraView::top);
    w.click(-80, -80, 0);
    press(w.rail("solid.create.cylinder"));
    require(w.session.validBodies().size() == 3, "Three valid bodies, no join, no error");

    // Names in the tree: Körper 1..3 become A, B, C (one undo step each).
    w.rename(QStringLiteral("Körper 1"), QStringLiteral("A"));
    w.rename(QStringLiteral("Körper 2"), QStringLiteral("B"));
    w.rename(QStringLiteral("Körper 3"), QStringLiteral("C"));
    auto start = w.volumes();
    require(start.size() == 3 && same(start["A"], 2000) && same(start["B"], 2000),
            "A and B are 20 x 10 x 10; names A, B, C");

    // Hide and show C from its row and the context bar: stored, no undo step.
    w.selectBody(QStringLiteral("C"));
    const bool couldUndo = w.session.canUndo();
    press(w.context("body.visibility"));
    require(w.hidden("C") && viewport::ViewportTestAccess::bodyCount(*w.viewport) == 2 &&
                w.session.canUndo() == couldUndo,
            "C hidden: not shown, no undo step");
    press(w.context("body.visibility"));
    require(!w.hidden("C") && viewport::ViewportTestAccess::bodyCount(*w.viewport) == 3,
            "C shown again");

    // With B selected, a new XY sketch joins A, named A in the panel.
    w.selectBody(QStringLiteral("B"));
    w.plane("XY-Ebene");
    press(w.context("solid.create.sketch"));
    w.rectangle(45, 0, 0, 60, 10, 0);
    w.view(viewport::CameraView::top);
    w.click(55, 5, 0);
    press(w.context("solid.create.extrude_profiles"));
    w.choose("operation", QStringLiteral("Vereinigen"));
    w.choose("target", QStringLiteral("A"));
    w.type("distance", 5);
    w.ok();
    auto joined = w.volumes();
    require(joined.size() == 3 && same(joined["A"], 2000 + 750 - 250) &&
                same(joined["B"], start["B"]) && same(joined["C"], start["C"]),
            "A grew by the join; B and C unchanged");

    require(w.session.undo().status() == application::CommandStatus::committed &&
                same(w.volumes()["A"], 2000),
            "Undo removes the join");
    require(w.session.redo().status() == application::CommandStatus::committed &&
                same(w.volumes()["A"], 2500),
            "Redo repeats it");

    // B hidden, then Save, New, Open: bodies, names, visibility and IDs stay.
    w.selectBody(QStringLiteral("B"));
    press(w.context("body.visibility"));
    std::vector<std::uint64_t> ids;
    for (const auto& body : w.session.modelOutline().bodies)
        ids.push_back(body.id.value());
    w.file("document.save_as");
    w.file("document.new");
    require(w.session.features().empty(), "New document");
    w.file("document.open");
    std::vector<std::uint64_t> reopened;
    for (const auto& body : w.session.modelOutline().bodies)
        reopened.push_back(body.id.value());
    auto loaded = w.volumes();
    require(reopened == ids && loaded.size() == 3 && w.hidden("B") && !w.hidden("A") &&
                same(loaded["A"], 2500),
            "Save/Open keeps three bodies, their IDs, names and B hidden");
    require(viewport::ViewportTestAccess::bodyCount(*w.viewport) == 2, "B stays hidden");
    user.answer = QMessageBox::Discard;
    std::cout << "PASS M01 bodies through the shell (A-BODY-01, A-BODY-03)\n";
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
