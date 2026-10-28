// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "HoleToolController.hpp"
#include "MainWindowState.hpp"
#include "SketchExtrusionToolController.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>

#include <algorithm>
#include <variant>

// M08 holes at a clicked face point; N09b holes at the points of a selected face sketch.
namespace voelund::desktop {

void MainWindow::addHoleServices(modules::solid::SolidCommandServices& services) {
    services.hole = [this]() {
        const auto sketch = sketchForHoles();
        if (!impl_->selectedPlanarFace && !sketch)
            return;
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        if (sketch)
            impl_->holeTool->beginAtSketch(*sketch);
        else
            impl_->holeTool->begin(*impl_->selectedPlanarFace);
        syncShell();
    };
    services.canHole = [this]() {
        return (impl_->selectedPlanarFace.has_value() && impl_->shell().canSketch()) ||
               sketchForHoles().has_value();
    };
}

// N09b: the selected sketch, when it lies on a body face and has points.
std::optional<document::FeatureId> MainWindow::sketchForHoles() const {
    const auto selected = impl_->session.selectedFeature();
    if (!selected || impl_->shell().toolBlocksCommands() ||
        impl_->session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch)
        return std::nullopt;
    const auto snapshot = impl_->session.identifiedSnapshot().snapshot();
    const auto& sketch = snapshot.sketchParameters(*selected);
    const bool points = std::ranges::any_of(sketch.elements.values, [](const auto& element) {
        return std::holds_alternative<document::SketchPointGeometry>(element.geometry);
    });
    if (!sketch.supportBinding || !points)
        return std::nullopt;
    return selected;
}

} // namespace voelund::desktop
