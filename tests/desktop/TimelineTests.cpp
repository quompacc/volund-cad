#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M15 (A-HISTORY-01/02 through the timeline): a saved plate with a hole and a rounded edge is
// opened; the timeline shows its features with their IDs in document order. Right and Left,
// End and Home select steps from the keyboard; Enter opens the extrusion's panel. The end
// marker dragged into the gap after the extrusion rolls back there; the marker dragged back to
// the end rolls forward; neither is an undo step.
namespace {
using namespace voelund;
using test::require;

void settle() {
    for (int i = 0; i < 4; ++i)
        QApplication::processEvents();
}
std::vector<QToolButton*> steps(QWidget& window) {
    std::vector<QToolButton*> found;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible())
            found.push_back(step);
    std::ranges::sort(found, {}, [](const QToolButton* step) { return step->x(); });
    return found;
}
void key(QWidget& widget, int code) {
    QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    settle();
}
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
// A drag on the timeline's track, in its own coordinates.
void dragMarker(QWidget& track, QPoint from, QPoint to) {
    const auto send = [&](QEvent::Type type, QPoint at, Qt::MouseButton button,
                          Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(at), QPointF(track.mapToGlobal(at)), button, buttons,
                          Qt::NoModifier);
        QApplication::sendEvent(&track, &event);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 10; ++step)
        send(QEvent::MouseMove, from + (to - from) * step / 10, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    settle();
}

void run() {
    const auto directory = std::filesystem::temp_directory_path() / "voelund-m15-timeline";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "plate.volund";
    {
        application::CadSession builder;
        builder.newDocument();
        document::SketchParameters square{document::datumSketchPlane(document::DatumPlane::xy),
                                          {}};
        square.datum = document::DatumPlane::xy;
        square.elements.values.push_back(
            {document::SketchElementId{1}, document::SketchRectangle{0, 0, 40, 20}});
        square.elements.nextId = 2;
        require(builder.createDatumSketchExtrusion(
                           square, features::identifySketchRegions(square.elements.values).ids, 10)
                        .status() == application::CommandStatus::committed,
                "Plate");
        const auto extrusion = builder.features().back().id;
        std::optional<document::FilletEdge> edge;
        for (TopExp_Explorer explorer(builder.validBodies().front().shape, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
            const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
            if (std::abs(middle.Y()) < 1e-9 && std::abs(middle.Z() - 10) < 1e-9)
                edge = builder.filletEdge(extrusion, explorer.Current());
        }
        require(edge && builder.createFillet(extrusion, std::span{&*edge, 1}, 2).status() ==
                            application::CommandStatus::committed,
                "Rounded edge");
        builder.saveDocument(path);
    }

    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    window.openFile(QString::fromStdString(path.string()));
    settle();

    const auto features = session.features();
    auto shown = steps(window);
    require(shown.size() == features.size(), "One step per feature");
    for (std::size_t index = 0; index < features.size(); ++index)
        require(shown[index]->property("featureId").toULongLong() == features[index].id.value() &&
                    shown[index]->accessibleName().contains(
                        QString::fromStdString(features[index].name)),
                "The steps are the document's features with their IDs, in order");

    test::activate(window);
    shown[0]->setFocus();
    key(*shown[0], Qt::Key_Right);
    require(session.selectedFeature() == features[1].id, "Right selects the next step");
    key(*steps(window)[1], Qt::Key_End);
    require(session.selectedFeature() == features.back().id, "End selects the last step");
    key(*steps(window).back(), Qt::Key_Home);
    require(session.selectedFeature() == features.front().id, "Home selects the first step");
    key(*steps(window)[1], Qt::Key_Return);
    require(panel(window) != nullptr, "Enter opens the extrusion's panel");
    auto* cancel = panel(window)->findChild<QPushButton*>(QStringLiteral("toolCancelButton"));
    require(cancel != nullptr, "Cancel button");
    cancel->click();
    settle();

    const bool undo = session.canUndo();
    auto* track = window.findChild<QWidget*>(QStringLiteral("timelineContent"));
    auto* end = window.findChild<QToolButton*>(QStringLiteral("timelineEnd"));
    require(track != nullptr && end != nullptr, "Timeline track and end marker");
    shown = steps(window);
    const int y = shown[0]->geometry().center().y();
    const QPoint gap((shown[1]->geometry().right() + shown[2]->geometry().left()) / 2, y);
    dragMarker(*track, QPoint(end->geometry().center().x(), y), gap);
    require(session.rollbackAfter() == features[1].id && session.canUndo() == undo &&
                steps(window)[2]->property("featureState") == QStringLiteral("rolledBack"),
            "The end marker dragged after the extrusion rolls back there");
    dragMarker(*track, gap, QPoint(end->geometry().center().x(), y));
    require(!session.rollbackAfter() && session.canUndo() == undo &&
                steps(window)[2]->property("featureState").toString().isEmpty(),
            "Dragged back to the end it rolls forward");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M15 timeline: steps are the saved features in order, keyboard selection "
                 "and Enter, marker dragged back and forward without undo steps"
              << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
