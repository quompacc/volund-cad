// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../../src/viewport/PlanarFaceSelection.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include "ModelTreeLookup.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <TopExp_Explorer.hxx>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

using namespace voelund;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void committed(const application::CommandResult& result,
               const std::source_location location = std::source_location::current()) {
    if (result.status() != application::CommandStatus::committed)
        throw std::runtime_error("Command failed at line " + std::to_string(location.line()) +
                                 ": " + result.diagnostic());
}
viewport::PlanarFaceSelection top(const application::CadSession& session,
                                  document::FeatureId feature) {
    const auto shape = session.featureShape(feature);
    require(shape.has_value(), "Missing support");
    for (TopExp_Explorer faces(*shape, TopAbs_FACE); faces.More(); faces.Next()) {
        auto candidate = viewport::planarFaceSelection(feature.value(), faces.Current());
        if (candidate && candidate->normalZ > 0.999) {
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(faces.Current(), properties);
            const auto center = properties.CentreOfMass();
            candidate->originX = center.X();
            candidate->originY = center.Y();
            candidate->originZ = center.Z();
            return *candidate;
        }
    }
    throw std::runtime_error("No planar top");
}
QPushButton* ok(QWidget& widget) {
    for (auto* b : widget.findChildren<QPushButton*>())
        if (b->objectName() == "toolAcceptButton")
            return b;
    throw std::runtime_error("No OK button");
}
void lines(desktop::SketchEditorToolController& editor) {
    editor.confirmElement({2, 2, 6, 2, {}, {}, {}, {}, {}, {}});
    editor.confirmElement({6, 2, 6, 6, {}, {}, {}, {}, {}, {}});
    editor.confirmElement({6, 6, 2, 6, {}, {}, {}, {}, {}, {}});
    editor.confirmElement({2, 6, 2, 2, {}, {}, {}, {}, {}, {}});
}
void screenshot(application::CadSession& session, const QString& name, bool failed) {
    desktop::MainWindow window(session);
    window.resize(1300, 850);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    QElapsedTimer timer;
    timer.start();
    while (!viewport->isRenderingReady() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    require(viewport->isRenderingReady(), "Viewport not ready");
    if (!failed) {
        QAction* edit = nullptr;
        for (auto* action : window.findChildren<QAction*>())
            if (action->property("commandId").toString() == "solid.modify.edit_sketch")
                edit = action;
        require(edit && edit->isEnabled(), "Edit action unavailable");
        edit->trigger();
        QApplication::processEvents();
    }
    if (failed) {
        auto* tree = window.findChild<QTreeWidget*>();
        require(tree != nullptr, "Model tree missing");
        const auto row = [&](std::size_t index) { return test::featureRow(*tree, session, index); };
        require(row(2)->text(0).contains("[Fehler]") && row(3)->text(0).contains("[blockiert]") &&
                    !row(4)->text(0).contains("[Fehler]"),
                "Support error tree incorrect");
        tree->setCurrentItem(row(2));
    }
    const auto directory = qEnvironmentVariable("VOELUND_SUPPORT_FLOW_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "Capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/" + name + ".png"), "Capture failed");
    }
}
void flow(const std::filesystem::path& directory) {
    application::CadSession session;
    session.newDocument();
    const auto a = test::requireCommittedFeature(session.createBoxFeature());
    const auto b = test::requireCommittedFeature(session.createBoxFeature());
    auto bp = session.selectedBoxParameters();
    bp.placement.translationX = 70;
    committed(session.editSelectedBoxParameters(bp));
    const auto selected = top(session, a);
    const auto other = top(session, b);
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    viewport::CadViewport viewport;
    QString status;
    desktop::SketchEditorToolController editor(
        session, viewport, *panel, [] {}, [&](const QString& s) { status = s; });
    auto foreign = selected;
    foreign.selectedFace = other.selectedFace;
    editor.begin(foreign);
    require(!editor.isActive(), "Coplanar foreign body was accepted");
    editor.begin(selected);
    require(editor.isActive(), "Picked support rejected");
    lines(editor);
    ok(*panel)->click();
    require(!editor.isActive() && session.features().size() == 3, "Product sketch creation failed");
    const auto sketch = *session.selectedFeature();
    const auto stored = session.sketchParameters(sketch);
    require(stored.supportBinding && stored.supportBinding->reference.origin.feature == a &&
                stored.supportBinding->reference.body.value() == a.value(),
            "Pick lost body/origin");
    require(session.selectFeature(a), "Select box");
    auto ap = session.selectedBoxParameters();
    ap.dimensions.height = 50;
    ap.placement.rotationY = kernel::Radians{std::numbers::pi / 2};
    committed(session.editSelectedBoxParameters(ap));
    editor.begin(selected);
    require(!editor.isActive(), "Stale topology accepted");
    require(session.selectFeature(sketch), "Select sketch");
    const auto moved = session.sketchParametersForEditing(sketch);
    require(std::abs(moved.plane.normalX - 1) < 1e-7 && moved.plane != stored.plane,
            "Editor did not rotate");
    const auto overlays = session.selectedSketchElements();
    const auto expected = features::evaluateSketchElements(moved);
    require(overlays.size() == expected.size() && overlays.front().points.front().world.Distance(
                                                      expected.front().points.front().world) < 1e-7,
            "Overlay differs from resolved geometry");
    require(editor.beginSelectedSketch(), "Reopen bound sketch");
    require(session.selectFeature(a), "Select support during draft");
    committed(session.editSelectedFeaturePlacement({}));
    ok(*panel)->click();
    require(editor.isActive(), "Stale editor committed");
    require(session.selectFeature(sketch), "Select sketch after stale draft");
    editor.cancel();
    require(session.selectFeature(a), "Select rotated box");
    committed(session.editSelectedBoxParameters(ap));
    require(session.selectFeature(sketch), "Select restored sketch");
    screenshot(session, "bound-rotated", false);
    const auto path = directory / "r04c-bound.volund";
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    require(loaded.sketchParameters(sketch) == session.sketchParameters(sketch) &&
                loaded.documentId() == session.documentId(),
            "Product save/open lost binding");
    require(loaded.selectFeature(sketch), "Select loaded sketch");
    require(loaded.selectedSketchElements().front().points.front().world.Distance(
                session.selectedSketchElements().front().points.front().world) < 1e-7,
            "Reopened overlay moved");
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select rotated profile");
    test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketch, session.selectedSketchProfiles(), 3));
    std::cout << "picked identity, coplanar body rejection, rotated overlay/editor, stale draft, "
                 "native roundtrip and rectangle extrusion passed\n";
}
void deletion(const std::filesystem::path& directory) {
    application::CadSession session;
    session.newDocument();
    test::requireCommittedFeature(session.createBoxFeature());
    const auto cut = test::requireCommittedFeature(
        session.createCylinderFeature(document::CylinderResultMode::cut));
    auto cp = session.selectedCylinderParameters();
    cp.radius = 2;
    cp.height = 5;
    committed(session.editSelectedCylinderParameters(cp));
    auto picked = top(session, cut);
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    viewport::CadViewport viewport;
    QString status;
    desktop::SketchEditorToolController editor(
        session, viewport, *panel, [] {}, [&](const QString& s) { status = s; });
    editor.begin(picked);
    require(editor.isActive(), "Cut top pick failed");
    lines(editor);
    ok(*panel)->click();
    require(!editor.isActive(), "Create cut sketch");
    const auto sketch = *session.selectedFeature();
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select profile");
    test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketch, session.selectedSketchProfiles(), 2));
    const auto independent = test::requireCommittedFeature(session.createBoxFeature());
    auto bp = session.selectedBoxParameters();
    bp.placement.translationX = 40;
    committed(session.editSelectedBoxParameters(bp));
    cp.radius = 100;
    const auto failure =
        session.editCylinderParameters(cut, cp, application::ErrorDisposition::keepEditableErrors);
    require(failure.status() == application::CommandStatus::committedWithErrors,
            "Support deletion not published");
    require(session.featureStates()[2].status == application::FeatureRuntimeStatus::error &&
                session.featureStates()[3].status == application::FeatureRuntimeStatus::blocked &&
                session.featureShape(independent).has_value(),
            "Deletion did not isolate branch");
    require(session.selectFeature(sketch), "Select failed sketch");
    require(session.selectedSketchElements().empty(), "Failed sketch exposed stale overlay");
    require(!editor.beginSelectedSketch() && status.contains("deleted"),
            "Failed support reopened on guessed plane");
    screenshot(session, "bound-deleted", true);
    const auto path = directory / "r04c-deleted.volund";
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    require(loaded.featureStates()[2].status == application::FeatureRuntimeStatus::error,
            "Reopen healed missing support");
    committed(session.undo());
    require(session.sceneStatus().isCurrent(), "Undo did not restore support");
    require(session.redo().status() == application::CommandStatus::committedWithErrors,
            "Redo lost support error");
    std::cout << "deleted support, blocked extrusion, independent body, hidden stale overlay, "
                 "tree, save/open and undo/redo passed\n";
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        require(argc == 2, "Fixture directory required");
        flow(argv[1]);
        deletion(argv[1]);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
