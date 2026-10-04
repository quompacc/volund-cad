#include "../../src/desktop/ModelTreePanel.hpp"
#include "../CommandResultAssertions.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QTreeWidget>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// U06: the model tree projects the real document — document origin with planes and grid,
// bodies with their own origin and features, German default names — and its eyes change
// only the view: no undo step and no unsaved state.
namespace {
using namespace voelund;
using test::require;
using Kind = desktop::ModelTreePanel::Kind;

QTreeWidgetItem* find(QTreeWidget& tree, Kind kind, const QString& text = {}) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->data(0, desktop::ModelTreePanel::kindRole).toInt() == static_cast<int>(kind) &&
            (text.isEmpty() || (*it)->text(0) == text))
            return *it;
    throw std::runtime_error("Tree row missing: " + text.toStdString());
}

QAction& command(desktop::MainWindow& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}

void run() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        document::SketchElements{
            {{document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    const auto sketchId = test::requireCommittedFeature(session.createDatumSketchFeature(sketch));
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false), "region");
    test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketchId, session.selectedSketchProfiles(), 5));
    test::requireCommittedFeature(session.createBoxFeature());

    const auto outline = session.modelOutline();
    require(outline.bodies.size() == 2 && outline.bodies[0].features.size() == 2 &&
                outline.bodies[1].features.size() == 1 && outline.loose.empty(),
            "Outline groups the sketch and its extrusion into one body, the box into another");

    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(tree != nullptr && tree->topLevelItemCount() == 3, "Origin and two bodies");
    require(tree->topLevelItem(0)->text(0) == QStringLiteral("Ursprung") &&
                tree->topLevelItem(1)->text(0) == QStringLiteral("Körper 1") &&
                tree->topLevelItem(2)->text(0) == QStringLiteral("Körper 2"),
            "Top level reads Ursprung, Körper 1, Körper 2");
    QTreeWidgetItem* first = tree->topLevelItem(1);
    require(first->childCount() == 3 && first->child(0)->text(0) == QStringLiteral("Ursprung") &&
                first->child(1)->text(0) == QStringLiteral("Skizze 1") &&
                first->child(2)->text(0) == QStringLiteral("Extrusion 1") &&
                tree->topLevelItem(2)->child(1)->text(0) == QStringLiteral("Quader 1"),
            "Bodies carry their origin and German feature names");
    require(viewport->displayedBasePlaneCount() == 0, "Origin hidden beside bodies");

    const auto stamp = session.sceneStatus().document;
    const bool modified = session.isModified();
    const bool canUndo = session.canUndo();
    const auto unchanged = [&] {
        return session.sceneStatus().document == stamp && session.isModified() == modified &&
               session.canUndo() == canUndo;
    };
    emit tree->itemClicked(find(*tree, Kind::grid), 1);
    require(!viewport->isGridVisible() && unchanged(), "Grid eye is view state");
    emit tree->itemClicked(find(*tree, Kind::bodyOrigin), 1);
    require(viewport->displayedBasePlaneCount() == 3 && unchanged(),
            "A body origin shows the planes");
    emit tree->itemClicked(find(*tree, Kind::plane, QStringLiteral("XZ-Ebene")), 1);
    require(viewport->displayedBasePlaneCount() == 2 && unchanged(), "Plane eye hides one plane");
    emit tree->itemClicked(find(*tree, Kind::body, QStringLiteral("Körper 2")), 1);
    require(unchanged(), "Body eye is view state");

    // A plane row picks the plane like the viewport does.
    emit tree->itemClicked(find(*tree, Kind::plane, QStringLiteral("YZ-Ebene")), 0);
    QApplication::processEvents();
    require(command(window, "solid.create.sketch").isEnabled(), "Tree pick enables Sketch");
    std::cout << "PASS model tree projection and view state\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VoelundTest"));
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
