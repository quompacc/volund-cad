#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M19 (user decision 2026-10-14): pick XY, "Bezugsebene", offset 25 (the preview shows the
// plane), 30, 35, OK; the timeline reopens it; undo; a click on the plane selects it and
// "Skizze" opens the editor on it; "Bezugsachse" on base Z; Save/Open keeps both.
namespace {
using namespace voelund;
using test::require;

QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& button(QWidget& window, const char* name) {
    auto* found = panel(window)->findChild<QPushButton*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}
template <class Field> Field* field(QWidget& window, const QString& name) {
    Field* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<Field*>(name))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Panel field");
    return found;
}
void setLength(QWidget& window, const char* id, double value) {
    field<QDoubleSpinBox>(window, QStringLiteral("toolLength_") + QLatin1String(id))
        ->setValue(value);
    settle();
}
QToolButton& timelineStep(QWidget& window, int index) {
    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == index)
            return *step;
    throw std::runtime_error("Timeline step missing");
}
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
double planeHeight(const application::CadSession& session) {
    for (const auto& feature : session.features())
        if (feature.kind == document::FeatureKind::datumPlane)
            if (const auto frame = session.datumFrame(feature.id))
                return frame->originZ;
    return -1;
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    window.refreshAfterExternalChange();

    // Pick XY in the tree, then "Bezugsebene".
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        if ((*it)->text(0) == QStringLiteral("XY-Ebene"))
            emit tree->itemClicked(*it, 0);
    settle();
    command(window, "solid.create.datum_plane").trigger();
    settle();
    require(panel(window) != nullptr, "The plane panel opens");
    for (const double offset : {25.0, 30.0, 25.0}) {
        setLength(window, "offset", offset);
        test::render(*viewport);
        require(viewport->displayedDatumCount() == 1, "The preview shows the plane");
    }
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 1 &&
                std::abs(planeHeight(session) - 25) < 1e-9 &&
                viewport->displayedDatumCount() == 1,
            "OK creates the plane 25 above XY");

    // The timeline reopens it; two changes in one session, one undo step.
    timelineStep(window, 0).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the plane");
    setLength(window, "offset", 30);
    setLength(window, "offset", 35);
    button(window, "toolAcceptButton").click();
    settle();
    require(std::abs(planeHeight(session) - 35) < 1e-9, "The plane moves to 35");
    require(session.undo().status() == application::CommandStatus::committed &&
                std::abs(planeHeight(session) - 25) < 1e-9,
            "Undo returns to 25");
    window.refreshAfterExternalChange();

    // A click on the plane selects it; "Skizze" opens the editor on it.
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 5, 5, 25);
    require(session.selectedFeature() == session.features().front().id,
            "A click selects the plane");
    QAction& sketch = command(window, "solid.create.sketch");
    require(sketch.isEnabled(), "Skizze is offered on the plane");
    sketch.trigger();
    settle();
    require(panel(window) != nullptr && !command(window, "sketch.tool.line").property("commandId")
                                            .toString()
                                            .isEmpty(),
            "The sketch editor opens on the plane");
    panel(window)->findChild<QPushButton*>(QStringLiteral("toolCancelButton"))->click();
    settle();

    // "Bezugsachse" without a picked edge: base Z in the panel.
    command(window, "solid.create.datum_axis").trigger();
    settle();
    require(panel(window) != nullptr, "The axis panel opens");
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2 &&
                session.features().back().kind == document::FeatureKind::datumAxis &&
                viewport->displayedDatumCount() == 2,
            "OK creates the Z axis");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m19-datum-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "datum.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    test::render(*viewport);
    require(session.features().size() == 2 && std::abs(planeHeight(session) - 25) < 1e-9 &&
                viewport->displayedDatumCount() == 2,
            "Open restores plane and axis");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M19 datum panel: plane by offset over XY, preview, edit, undo, pick, "
                 "sketch on it, axis, open\n";
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
