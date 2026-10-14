#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "SketchEditorToolController.hpp"
#include "SketchExtrusionToolController.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QStatusBar>

#include <algorithm>
#include <vector>

// The tool controllers and the edges picked before a tool (user request 2026-10-13).
namespace voelund::desktop {

void MainWindow::createToolPanel() {
    impl_->toolPanel = new ToolPanelHost(impl_->viewport);
    impl_->sketchEditorTool = std::make_unique<SketchEditorToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->sketchExtrusionTool = std::make_unique<SketchExtrusionToolController>(
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    const auto showPreview = [this](const std::vector<features::EvaluatedBody>* bodies) {
        if (bodies == nullptr) {
            syncVisibility(); // the model's bodies again
            return;
        }
        std::vector<viewport::SceneBody> scene;
        for (const auto& body : *bodies)
            scene.push_back({body.sourceFeature.value(), body.shape, false, false});
        impl_->viewport->setBodies(scene);
    };
    impl_->sketchExtrusionTool->setPreviewHandler([this, showPreview](const auto* bodies) {
        syncSketchPresentation(); // M04: contour elements of an invalid region turn red
        showPreview(bodies);
    });
    impl_->pushPullTool = std::make_unique<PushPullToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->filletTool = std::make_unique<FilletToolController>(
        impl_->session, *impl_->viewport, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->filletTool->setPreviewHandler(showPreview);
    impl_->holeTool = std::make_unique<HoleToolController>( // M08
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->holeTool->setPreviewHandler(showPreview);
    impl_->combineTool = std::make_unique<CombineToolController>( // M21
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->combineTool->setPreviewHandler(showPreview);
    impl_->datumTool = std::make_unique<DatumToolController>( // M19
        impl_->session, *impl_->toolPanel, [this]() { refreshScene(); },
        [this](const QString& message) { statusBar()->showMessage(message); });
    impl_->datumTool->setPreviewHandler([this](const application::CadSession* candidate) {
        impl_->viewport->setDatums(makeSceneDatums(candidate ? *candidate : impl_->session));
    });
    impl_->viewport->setEdgeSelectionHandler(
        [this](std::uint64_t body, const TopoDS_Shape& edge, bool additive) {
            pickEdge(body, edge, additive);
        });
}

// User request 2026-10-13: edges are picked before a tool, as in any CAD. A click picks one
// edge of a body, Ctrl+click adds or removes one; "Verrunden"/"Fase" take them over. While a
// fillet or chamfer panel is open its own edge picking runs instead.
void MainWindow::pickEdge(std::uint64_t body, const TopoDS_Shape& edge, bool additive) {
    if (impl_->filletTool->isActive()) {
        impl_->filletTool->picked(body, edge);
        return;
    }
    if (impl_->shell().toolBlocksCommands())
        return;
    const document::FeatureId support{body};
    const auto named = impl_->session.filletEdge(support, edge);
    if (!named) {
        statusBar()->showMessage(
            tr("Diese Kante liegt nicht zwischen zwei benannten Flächen des Körpers."), 3000);
        return;
    }
    auto edges = additive && impl_->edgeSupport == support ? impl_->selectedEdges
                                                          : std::vector<document::FilletEdge>{};
    impl_->selectedPlanarFace.reset();
    impl_->selectedDatum.reset();
    impl_->selectedBody.reset();
    selectFeature(body);
    const auto known = std::ranges::find_if(
        edges, [&](const auto& other) { return features::sameFilletEdge(other, *named); });
    if (known != edges.end())
        edges.erase(known);
    else
        edges.push_back(*named);
    impl_->edgeSupport = support;
    impl_->selectedEdges = std::move(edges);
    const auto shapes = impl_->session.filletEdgeShapes(support, impl_->selectedEdges);
    impl_->viewport->setHighlightedEdges(shapes);
    syncShell();
    statusBar()->showMessage(tr("%1 Kante(n) gewählt").arg(impl_->selectedEdges.size()), 2000);
}

void MainWindow::clearEdgeSelection() {
    if (impl_->selectedEdges.empty())
        return;
    impl_->selectedEdges.clear();
    impl_->edgeSupport.reset();
    impl_->viewport->setHighlightedEdges({});
}

} // namespace voelund::desktop
