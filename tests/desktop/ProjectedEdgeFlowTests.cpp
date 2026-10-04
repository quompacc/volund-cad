#include "IsolatedSettings.hpp"
#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/SketchViewAdapter.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../../src/viewport/PlanarFaceSelection.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPushButton>
#include <QThread>
#include <TopExp_Explorer.hxx>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "ViewportLifecycleSupport.hpp"
#endif

using namespace voelund;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void committed(const application::CommandResult& result) {
    if (result.status() != application::CommandStatus::committed)
        throw std::runtime_error(result.diagnostic());
}
viewport::PlanarFaceSelection top(const application::CadSession& session,
                                  document::FeatureId feature) {
    for (TopExp_Explorer it(*session.featureShape(feature), TopAbs_FACE); it.More(); it.Next()) {
        auto face = viewport::planarFaceSelection(feature.value(), it.Current());
        if (face && face->normalZ > .999)
            return *face;
    }
    throw std::runtime_error("Top face missing");
}
document::SketchPlane plane(const viewport::PlanarFaceSelection& s) {
    return {s.originX, s.originY,     s.originZ,     s.normalX,    s.normalY,
            s.normalZ, s.xDirectionX, s.xDirectionY, s.xDirectionZ};
}
QPushButton* accept(QWidget& panel) {
    for (auto* button : panel.findChildren<QPushButton*>())
        if (button->objectName() == "toolAcceptButton")
            return button;
    throw std::runtime_error("Accept button missing");
}
void mouseSelection(const application::CadSession& session, document::FeatureId box,
                    const viewport::PlanarFaceSelection& support,
                    const application::ProjectedSketchPoint& point) {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    viewport::CadViewport view;
    view.resize(900, 700);
    view.show();
    test::render(view);
    const std::vector<viewport::SceneBody> bodies{{box.value(), *session.featureShape(box), false}};
    view.setBodies(bodies);
    view.fitView();
    std::optional<viewport::SketchElementIntent> selected;
    view.setSketchEditorElementHandler([&](const auto& intent) { selected = intent; });
    const gp_Vec x(support.xDirectionX, support.xDirectionY, support.xDirectionZ);
    const auto y = gp_Vec(support.normalX, support.normalY, support.normalZ).Crossed(x);
    const auto free = point.world.Translated(x * 7 + y * 11);
    for (bool reverse : {false, true}) {
        view.beginSketchEditor(support, viewport::SketchElementTool::line);
        view.setSketchElements({});
        test::render(view);
        auto click = [&](const gp_Pnt& world) {
            const auto pixel =
                viewport::ViewportTestAccess::project(view, world.X(), world.Y(), world.Z());
            require(view.rect().contains(pixel), "Snap point outside viewport");
            test::mouse(view, QEvent::MouseButtonPress, pixel, Qt::LeftButton, Qt::LeftButton);
            test::mouse(view, QEvent::MouseButtonRelease, pixel, Qt::LeftButton, Qt::NoButton);
        };
        selected.reset();
        click(reverse ? free : point.world);
        click(reverse ? point.world : free);
        require(selected.has_value(), "Mouse drawing did not produce intent");
        const auto& ref = reverse ? selected->secondReference : selected->firstReference;
        require(ref && ref->projectedEdge == point.reference,
                "Mouse selection lost typed first/second endpoint reference");
    }
#else
    (void)session;
    (void)box;
    (void)support;
    (void)point;
#endif
}
void capture(application::CadSession& session) {
    const auto dir = qEnvironmentVariable("VOELUND_EDGE_FLOW_ARTIFACT_DIR");
    if (dir.isEmpty())
        return;
    desktop::MainWindow window(session);
    window.resize(1300, 850);
    window.show();
    auto* view = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    QElapsedTimer timer;
    timer.start();
    while (!view->isRenderingReady() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    require(view->isRenderingReady(), "Viewport unavailable");
    bool triggered = false;
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == "solid.modify.edit_sketch" &&
            action->isEnabled()) {
            action->trigger();
            triggered = true;
            break;
        }
    require(triggered, "Edit Sketch unavailable");
    QApplication::processEvents();
    require(QDir().mkpath(dir), "Capture directory failed");
    require(window.grab().save(dir + "/projected-edge-editor.png"), "Capture failed");
}
void flow(const std::filesystem::path& dir) {
    application::CadSession session;
    committed(session.createBoxFeature());
    const auto box = *session.selectedFeature();
    const auto support = top(session, box);
    const auto binding = session.bindSelectedSupport(box, support.selectedFace, plane(support));
    const auto coarse = session.projectSketchSupport(box, binding, 4);
    const auto dense = session.projectSketchSupport(box, binding, 64);
    require(coarse.points.size() == 12 && dense.points.size() == 12, "Box boundary sample count");
    require(coarse.segments.size() == 16 && dense.segments.size() == 256,
            "Display density not applied");
    for (std::size_t i = 0; i < coarse.points.size(); ++i)
        require(coarse.points[i].reference == dense.points[i].reference &&
                    coarse.points[i].world.Distance(dense.points[i].world) < 1e-9,
                "Display density changed analytic selection");
    const auto selected = std::ranges::find_if(coarse.points, [](const auto& point) {
        const auto* origin =
            std::get_if<document::PrimitiveEdgeOrigin>(&point.reference.origin.value);
        return origin && origin->first == kernel::PrimitiveRole::boxNegativeY &&
               origin->second == kernel::PrimitiveRole::boxPositiveZ &&
               point.reference.selection.kind == document::EdgePointKind::parameter;
    });
    require(selected != coarse.points.end(), "Named midpoint missing");
    auto display = support;
    desktop::setSupportProjection(display, coarse);
    require(display.stableReferences && display.referencePoints.size() == 12,
            "Adapter lost stable points");
    mouseSelection(session, box, display, *selected);
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    viewport::CadViewport view;
    QString status;
    desktop::SketchEditorToolController controller(
        session, view, *panel, [] {}, [&](const QString& s) { status = s; });
    controller.begin(support);
    require(controller.isActive(), "Editor did not start");
    viewport::SketchElementIntent intent{selected->local.x,
                                         selected->local.y,
                                         selected->local.x + 7,
                                         selected->local.y + 11,
                                         {},
                                         {},
                                         {},
                                         {},
                                         {},
                                         {}};
    intent.firstReference = viewport::SketchPointReferenceIntent{
        0, viewport::SketchPointKind::projected, 0, 0, selected->reference};
    auto invalid = intent;
    invalid.firstReference->projectedEdge->body = document::BodyId{999};
    controller.confirmElement(invalid);
    require(status.contains("abgelehnt") && !accept(*panel)->isEnabled(),
            "Rejected attachment left an unbound draft element");
    controller.confirmElement(intent);
    require(accept(*panel)->isEnabled(), "Typed attachment failed");
    accept(*panel)->click();
    require(!controller.isActive(), "Accept did not close editor");
    const auto sketch = *session.selectedFeature();
    const auto saved = session.sketchParameters(sketch);
    require(saved.elements.values.size() == 1 && saved.constraints.values.size() == 2,
            "Unexpected sketch payload");
    for (const auto& constraint : saved.constraints.values)
        require(constraint.projectedEdge == selected->reference &&
                    constraint.auxiliaryValues.empty(),
                "Controller persisted legacy attachment");
    committed(session.undo());
    committed(session.redo());
    const auto path = dir / "projected-edge-ui.volund";
    session.saveDocument(path);
    session.loadDocument(path);
    require(session.sketchParameters(sketch).constraints == saved.constraints,
            "Save/Open changed references");
    require(session.selectFeature(box), "Select box");
    auto dimensions = session.selectedBoxDimensions();
    dimensions.width *= 1.5;
    committed(session.editSelectedBoxDimensions(dimensions));
    const auto resolved = session.sketchParametersForEditing(sketch);
    const auto& line = std::get<document::SketchLine>(resolved.elements.values.front().geometry);
    // Editing receives solved constraints; the editor must apply their recomputed coordinates.
    (void)line;
    application::SketchEditorSession editor(session, sketch, box, resolved);
    const auto draft = editor.draftParameters();
    const auto& solved = std::get<document::SketchLine>(draft.elements.values.front().geometry);
    require(std::abs(solved.start.x - draft.constraints.values[0].value) < 1e-7 &&
                std::abs(solved.start.y - draft.constraints.values[1].value) < 1e-7,
            "Reopened editor ignored analytic recompute");
    const auto before = editor.draftParameters();
    auto missing = selected->reference;
    std::get<document::PrimitiveEdgeOrigin>(missing.origin.value).feature =
        document::FeatureId{999};
    require(!editor.addProjectedEdgeReference(
                {document::SketchElementId{1}, document::SketchPointPosition::end}, missing),
            "Missing reference accepted");
    require(editor.draftParameters().constraints == before.constraints,
            "Rejected reference mutated constraints");
    committed(session.createBoxFeature());
    require(!editor.addProjectedEdgeReference({document::SketchElementId{1}}, selected->reference),
            "Stale draft accepted reference");
    require(session.selectFeature(sketch), "Select sketch");
    require(controller.beginSelectedSketch(), "Existing projected sketch could not reopen");
    controller.cancel();
    capture(session);
    std::cout << "typed editor flow, density invariance, rollback, persistence, recompute and "
                 "stale draft passed\n";
}
void circle() {
    application::CadSession session;
    committed(session.createCylinderFeature());
    const auto feature = *session.selectedFeature();
    const auto support = top(session, feature);
    const auto binding = session.bindSelectedSupport(feature, support.selectedFace, plane(support));
    const auto projection = session.projectSketchSupport(feature, binding, 7);
    require(projection.points.size() == 3 && projection.segments.size() == 7,
            "Circle boundary projection");
    const auto& start = projection.points[0].world;
    const auto& middle = projection.points[1].world;
    const auto& end = projection.points[2].world;
    require(start.Distance(end) < 1e-7 && start.Distance(middle) > 1,
            "Circle midpoint was interpolated on seam chord");
    std::cout << "analytic circle seam and midpoint passed\n";
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        require(argc == 2, "Expected artifact directory");
        flow(argv[1]);
        circle();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
