// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "BodyColorCard.hpp"
#include "BodyMaterialSection.hpp"
#include "CommandFeedback.hpp"
#include "DisplayStyleMenu.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"
#include "SelectionFilterMenu.hpp"

#include <voelund/application/CadSession.hpp>

#include <QAction>
#include <QStatusBar>

// M01 (user decision 2026-10-10): bodies as a whole. A body row in the model tree selects the
// body; its name is edited in place (double-click, F2 or "Umbenennen") and is one undo step;
// the eye and "Ausblenden" store the visibility with the document without an undo step.
namespace voelund::desktop {

void MainWindow::createBodyCommands() {
    impl_->renameBodyAction = new QAction(tr("Umbenennen"), this);
    setCommandId(*impl_->renameBodyAction, "body.rename");
    addAction(impl_->renameBodyAction);
    connect(impl_->renameBodyAction, &QAction::triggered, this, [this] {
        if (impl_->selectedBody)
            impl_->modelTree->editBody(*impl_->selectedBody);
    });
    impl_->bodyVisibilityAction = new QAction(tr("Ausblenden"), this);
    setCommandId(*impl_->bodyVisibilityAction, "body.visibility");
    impl_->bodyVisibilityAction->setShortcut(QKeySequence(QStringLiteral("V"))); // W04
    addAction(impl_->bodyVisibilityAction);
    connect(impl_->bodyVisibilityAction, &QAction::triggered, this, [this] {
        // N02b: a group shows or hides all its bodies.
        for (const auto body : selectedBodies())
            toggleVisibility(static_cast<int>(ModelTreePanel::Kind::body), body.value());
    });
    syncBodyCommands();
}

// N06 (decision 2026-10-23): a selected body shows its colour card in the slot of the feature
// properties; the display style sits beside the view cube.
void MainWindow::createBodyAppearance() {
    impl_->bodyCard = new BodyColorCard(*impl_->viewport);
    impl_->bodyCard->setChoiceHandler(
        [this](std::optional<std::uint32_t> color) { colorBodies(color); });
    impl_->bodyCard->material().setChoiceHandler(
        [this](std::optional<document::BodyMaterial> material) { giveMaterial(material); });
    impl_->displayStyle = new DisplayStyleMenu(*this, *impl_->viewport);
    impl_->selectionFilter = new SelectionFilterMenu(*this, *impl_->viewport); // W07
}

// One undo step per body; a group (N02b) takes the colour on each of its bodies.
void MainWindow::colorBodies(std::optional<std::uint32_t> color) {
    const auto bodies = selectedBodies();
    for (const auto body : bodies)
        if (!runCommand([&] { return impl_->session.setBodyColor(body, color); },
                        [this](const QString& message) { statusBar()->showMessage(message); },
                        true))
            break;
    refreshScene();
    if (!bodies.empty())
        selectBody(bodies.front().value(), true);
    statusBar()->showMessage(tr("Körperfarbe geändert"), 2000);
}

// N07: one undo step per body, like the colour; a group takes the material on each body.
void MainWindow::giveMaterial(std::optional<document::BodyMaterial> material) {
    const auto bodies = selectedBodies();
    for (const auto body : bodies)
        if (!runCommand([&] { return impl_->session.setBodyMaterial(body, material); },
                        [this](const QString& message) { statusBar()->showMessage(message); },
                        true))
            break;
    refreshScene();
    if (!bodies.empty())
        selectBody(bodies.front().value(), true);
    statusBar()->showMessage(material ? tr("Material zugewiesen") : tr("Material entfernt"), 2000);
}

void MainWindow::syncBodyCommands() {
    if (impl_->renameBodyAction == nullptr)
        return;
    const ShellState shell = impl_->shell();
    const bool body = shell.selection == ShellSelection::body && !shell.toolBlocksCommands();
    syncBodyCard(body);
    impl_->renameBodyAction->setEnabled(body && impl_->bodyGroup.size() < 2); // N02b
    impl_->bodyVisibilityAction->setEnabled(body);
    impl_->bodyVisibilityAction->setText(shell.bodyHidden ? tr("Einblenden") : tr("Ausblenden"));
    syncClipboardCommands();
}

void MainWindow::selectBody(std::uint64_t id, bool keepGroup) {
    if (impl_->shell().toolBlocksCommands())
        return;
    if (!keepGroup)
        clearGroups();
    impl_->selectedDatum.reset();
    impl_->selectedPlanarFace.reset();
    // The creator feature stands for the body in the document selection, so a cylinder or a
    // move targets this body (CadSession::selectedBodyProducer).
    static_cast<void>(impl_->session.selectFeature(document::FeatureId{id}));
    impl_->selectedBody = document::BodyId{id};
    impl_->modelTree->selectBody(document::BodyId{id});
    syncTimeline();
    syncProperties();
    syncSketchPresentation();
    syncTransformTool();
    syncShell();
    statusBar()->showMessage(tr("%1 gewählt").arg(impl_->shell().bodyName), 2000);
}

void MainWindow::renameBody(std::uint64_t id, const QString& name) {
    if (!runCommand(
            [&] { return impl_->session.renameBody(document::BodyId{id}, name.toStdString()); },
            [this](const QString& message) { statusBar()->showMessage(message); })) {
        syncModelTree(); // the row shows the stored name again
        return;
    }
    refreshScene();
    selectBody(id);
    statusBar()->showMessage(tr("Körper umbenannt"), 2000);
}

void MainWindow::syncBodyCard(bool shown) {
    if (impl_->bodyCard == nullptr)
        return;
    std::optional<std::uint32_t> color;
    for (const auto& item : impl_->session.modelOutline().bodies)
        if (impl_->selectedBody && item.id == *impl_->selectedBody)
            color = item.color;
    impl_->bodyCard->display(impl_->shell().bodyName, color);
    impl_->bodyCard->setVisible(shown && impl_->selectedBody.has_value());
    if (impl_->bodyCard->isVisible())
        showMassProperties();
}

namespace {
using MassCache = std::vector<std::pair<TopoDS_Shape, kernel::MassProperties>>;
kernel::MassProperties measured(MassCache& cache, const TopoDS_Shape& shape) {
    for (const auto& [cached, properties] : cache)
        if (cached.IsSame(shape))
            return properties;
    if (cache.size() >= 64) // bounded; old shapes are not shown any more
        cache.clear();
    return cache.emplace_back(shape, kernel::massProperties(shape)).second;
}
} // namespace

// N07: the selected bodies weighed together; a body without material counts for its geometry
// only (the mass is then not shown).
void MainWindow::showMassProperties() {
    const auto outline = impl_->session.modelOutline().bodies;
    const auto shapes = impl_->session.validBodies();
    std::vector<kernel::WeightedBody> weighed;
    std::optional<document::BodyMaterial> shown;
    bool everyMaterial = true;
    for (const auto body : selectedBodies()) {
        const auto item = std::ranges::find(outline, body, &application::OutlineBody::id);
        const auto shape = std::ranges::find(shapes, body, &features::EvaluatedBody::bodyId);
        if (shape == shapes.end())
            continue;
        const auto material = item != outline.end() ? item->material : std::nullopt;
        if (weighed.empty())
            shown = material;
        everyMaterial = everyMaterial && material.has_value();
        weighed.push_back(
            {measured(impl_->massCache, shape->shape), material ? material->density : 1.0});
    }
    impl_->bodyCard->material().display(shown, kernel::combineMasses(weighed),
                                        everyMaterial && !weighed.empty());
}

} // namespace voelund::desktop
