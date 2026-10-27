// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "InterferenceToolController.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "MeasureToolController.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/modules/solid/SolidWorkspaceModule.hpp>

#include <QStatusBar>

#include <algorithm>

// M12 measuring and N08 interference: tools that look at the model and change nothing.
namespace voelund::desktop {

void MainWindow::createInspectTools() {
    const auto status = [this](const QString& message) { statusBar()->showMessage(message); };
    impl_->measureTool = std::make_unique<MeasureToolController>(impl_->session, *impl_->viewport,
                                                                 *impl_->toolPanel, status);
    impl_->interferenceTool =
        std::make_unique<InterferenceToolController>(*impl_->viewport, *impl_->toolPanel, status);
}

void MainWindow::addInspectServices(modules::solid::SolidCommandServices& services) {
    // M12: measuring changes nothing; it needs a current model with a body.
    services.measure = [this]() {
        if (impl_->sketchExtrusionTool->isActive())
            impl_->sketchExtrusionTool->cancel();
        clearEdgeSelection();
        impl_->measureTool->begin();
        syncShell();
    };
    const auto canInspect = [this]() {
        return impl_->session.sceneStatus().isCurrent() && !impl_->session.validBodies().empty() &&
               !impl_->shell().toolBlocksCommands();
    };
    services.canMeasure = canInspect;
    services.interference = [this]() { checkInterference(); };
    services.canInterference = canInspect;
}

// N08: two or more selected bodies against each other, otherwise all shown bodies.
void MainWindow::checkInterference() {
    if (impl_->sketchExtrusionTool->isActive())
        impl_->sketchExtrusionTool->cancel();
    clearEdgeSelection();
    const auto chosen = selectedBodies();
    const bool selected = chosen.size() >= 2;
    const auto outline = impl_->session.modelOutline();
    std::vector<InterferenceToolController::Body> bodies;
    for (const auto& body : impl_->session.validBodies()) {
        const bool wanted = selected
                                ? std::ranges::find(chosen, body.bodyId) != chosen.end()
                                : !impl_->visibility.hiddenBodies.contains(body.bodyId.value());
        if (wanted)
            bodies.push_back({bodyLabel(outline, body.bodyId), body.shape});
    }
    impl_->interferenceTool->begin(std::move(bodies), selected);
    syncShell();
}

} // namespace voelund::desktop
