// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "MainWindowState.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

// N12a/N12b/N13b: reopening a step in its panel, and the construction commands' services.
namespace voelund::desktop {

// The panel that edits a step of this kind, opened on it (N12a: a table instead of a chain of
// ifs); a pattern copy or split part opens its owner. False for kinds without one.
bool MainWindow::beginToolEdit(document::FeatureId selected, document::FeatureKind kind) {
    using Kind = document::FeatureKind;
    const auto snapshot = impl_->session.identifiedSnapshot().snapshot();
    const std::function<void(document::FeatureId)> fillet = [this](auto id) {
        impl_->filletTool->beginEdit(id);
    };
    const std::function<void(document::FeatureId)> datum = [this](auto id) {
        impl_->datumTool->beginEdit(id);
    };
    const std::pair<Kind, std::function<void(document::FeatureId)>> editors[]{
        {Kind::fillet, fillet},
        {Kind::chamfer, fillet},
        {Kind::hole, [this](auto id) { impl_->holeTool->beginEdit(id); }},
        {Kind::combine, [this](auto id) { impl_->combineTool->beginEdit(id); }},
        {Kind::shell, [this](auto id) { impl_->shellTool->beginEdit(id); }},
        {Kind::draft, [this](auto id) { impl_->draftTool->beginEdit(id); }},     // M25
        {Kind::mirror, [this](auto id) { impl_->mirrorTool->beginEdit(id); }},   // M09
        {Kind::move, [this](auto id) { impl_->moveTool->beginEdit(id); }},       // M11
        {Kind::pattern, [this](auto id) { impl_->patternTool->beginEdit(id); }}, // M10
        {Kind::patternCopy,
         [&](auto id) {
             impl_->patternTool->beginEdit(snapshot.patternCopyParameters(id).pattern);
         }},
        {Kind::datumPlane,
         [&](auto id) {
             const auto& plane = snapshot.datumPlaneParameters(id);
             if (plane.secondFace || !plane.points.empty()) // N13b
                 impl_->constructionTool->beginEdit(id);
             else
                 impl_->datumTool->beginEdit(id);
         }},
        {Kind::datumPoint, [this](auto id) { impl_->constructionTool->beginEdit(id); }},
        {Kind::datumAxis, datum},
        {Kind::split, [this](auto id) { impl_->splitTool->beginEdit(id); }},       // N12a
        {Kind::extrusion, [this](auto id) { impl_->ribTool->beginEdit(id); }},     // N12b: a rib
        {Kind::faceEdit, [this](auto id) { impl_->faceEditTool->beginEdit(id); }}, // N13a
        {Kind::splitPart,
         [&](auto id) { impl_->splitTool->beginEdit(snapshot.splitPartParameters(id).split); }}};
    using Editor = std::pair<Kind, std::function<void(document::FeatureId)>>;
    const auto editor = std::ranges::find(editors, kind, &Editor::first);
    if (editor == std::end(editors))
        return false;
    editor->second(selected);
    syncShell();
    return true;
}

// N13b: midplane, plane through three points, construction point.
void MainWindow::addConstructionServices(modules::solid::SolidCommandServices& services) {
    const auto opened = [this] {
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        syncShell();
    };
    services.midplane = [this, opened]() {
        if (!impl_->selectedPlanarFace)
            return;
        impl_->constructionTool->beginMidplane(*impl_->selectedPlanarFace);
        opened();
    };
    services.canMidplane = [this]() {
        return impl_->selectedPlanarFace.has_value() && !impl_->shell().toolBlocksCommands();
    };
    services.planeThroughPoints = [this, opened]() {
        impl_->constructionTool->beginPlaneThroughPoints();
        opened();
    };
    services.canPlaneThroughPoints = [this]() {
        return std::ranges::count(impl_->session.features(), document::FeatureKind::datumPoint,
                                  &document::FeatureDescriptor::kind) >= 3 &&
               !impl_->shell().toolBlocksCommands();
    };
    services.datumPoint = [this, opened]() {
        const bool oneEdge = impl_->selectedEdges.size() == 1 && impl_->edgeSupport;
        impl_->constructionTool->beginPoint(oneEdge ? impl_->edgeSupport : std::nullopt,
                                            oneEdge ? std::optional{impl_->selectedEdges.front()}
                                                    : std::nullopt);
        clearEdgeSelection();
        opened();
    };
}

bool MainWindow::isRib(document::FeatureId feature) const {
    return impl_->session.identifiedSnapshot()
        .snapshot()
        .extrusionParameters(feature)
        .rib.has_value();
}

} // namespace voelund::desktop
