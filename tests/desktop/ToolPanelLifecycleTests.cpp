#include "IsolatedSettings.hpp"
#include "../CommandResultAssertions.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeySequence>
#include <QPushButton>
#include <QShortcut>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U05 / A-UI-02 (Sketch, Extrude): the floating tool panel starts, takes a parameter change,
// and Cancel or Escape restore the exact starting state; OK commits exactly one transaction.
// While a tool is open, other tools, primitives and document undo wait.
namespace {
using namespace voelund;
using test::require;

QAction& command(desktop::MainWindow& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}

struct Snapshot final {
    application::DocumentStamp stamp;
    bool modified;
    bool canUndo;
    std::size_t features;
};

Snapshot snapshot(const application::CadSession& session) {
    return {session.sceneStatus().document, session.isModified(), session.canUndo(),
            session.features().size()};
}

bool same(const Snapshot& a, const Snapshot& b) {
    return a.stamp == b.stamp && a.modified == b.modified && a.canUndo == b.canUndo &&
           a.features == b.features;
}

void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}

void run() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        document::SketchElements{
            {{document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    test::requireCommittedFeature(session.createDatumSketchFeature(sketch));
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select the region");
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* properties = window.findChild<QWidget*>(QStringLiteral("featurePropertiesCard"));
    require(panel != nullptr && properties != nullptr, "Floating cards exist");
    require(panel->parentWidget() == viewport && properties->parentWidget() == viewport,
            "Cards float in the viewport");
    require(!panel->isVisible() && properties->isVisible(),
            "Without a tool the selected feature's properties are shown");
    const Snapshot start = snapshot(session);

    QAction& extrude = command(window, "solid.create.extrude_profiles");
    // Start, change a parameter, Cancel: identical starting state.
    extrude.trigger();
    settle();
    require(panel->isVisible() && !properties->isVisible(), "Tool panel replaces properties");
    for (auto* input : panel->findChildren<QDoubleSpinBox*>())
        input->setValue(7.0);
    require(!command(window, "solid.create.box").isEnabled() &&
                !command(window, "solid.modify.edit_sketch").isEnabled() &&
                !command(window, "history.undo").isEnabled(),
            "An open tool blocks other tools, primitives and document undo");
    panel->findChild<QPushButton*>(QStringLiteral("toolCancelButton"))->click();
    settle();
    require(!panel->isVisible() && same(start, snapshot(session)),
            "Cancel restores the starting state");

    // Escape behaves like Cancel.
    extrude.trigger();
    settle();
    require(panel->isVisible(), "Extrusion restarts");
    bool escaped = false;
    for (auto* shortcut : panel->findChildren<QShortcut*>())
        if (shortcut->key() == QKeySequence(Qt::Key_Escape)) {
            emit shortcut->activated();
            escaped = true;
        }
    require(escaped, "The panel registers Escape");
    settle();
    require(!panel->isVisible() && same(start, snapshot(session)),
            "Escape cancels without changes");

    // OK commits exactly one transaction.
    extrude.trigger();
    settle();
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(!panel->isVisible() && session.features().size() == start.features + 1 &&
                session.canUndo(),
            "OK commits the extrusion");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.features().size() == start.features,
            "One undo removes the extrusion");
    std::cout << "PASS floating tool panel lifecycle\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
