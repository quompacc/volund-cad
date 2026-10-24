// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"
#include "TimelineBar.hpp"

#include <QStatusBar>

#include <algorithm>

// N02b (user decision 2026-10-21): Strg-click adds or removes, Umschalt-click adds a range -
// bodies in the model tree and in the view (no tool open), steps in the timeline. Delete,
// suppress, hide, copy and move act on the whole group as one undo step. A plain click starts
// over with one item.
namespace voelund::desktop {
namespace {
// W07: the bodies (once each) whose shown results are among `shown`.
template <typename Bodies>
std::vector<document::BodyId> bodiesShown(const Bodies& displayed,
                                          const std::vector<std::uint64_t>& shown) {
    std::vector<document::BodyId> bodies;
    for (const auto& body : displayed)
        if (std::ranges::find(shown, body.sourceFeature.value()) != shown.end() &&
            std::ranges::find(bodies, body.bodyId) == bodies.end())
            bodies.push_back(body.bodyId);
    return bodies;
}

// W07: a box adds its bodies to the group; with Ctrl the body selected before stays in it.
template <typename Selected>
void extendGroup(std::vector<document::BodyId>& group, const Selected& selected,
                 const std::vector<document::BodyId>& bodies, bool additive) {
    if (additive && group.empty() && selected)
        group.push_back(*selected);
    for (const auto body : bodies)
        if (std::ranges::find(group, body) == group.end())
            group.push_back(body);
}

template <class Id> void toggle(std::vector<Id>& group, Id id) {
    if (const auto found = std::ranges::find(group, id); found != group.end())
        group.erase(found);
    else
        group.push_back(id);
}

// Umschalt: everything between the last chosen and `id` in `order` joins the group.
template <class Id> void addRange(std::vector<Id>& group, Id id, const std::vector<Id>& order) {
    const auto anchor = group.empty() ? order.end() : std::ranges::find(order, group.back());
    const auto target = std::ranges::find(order, id);
    if (anchor == order.end() || target == order.end()) {
        if (std::ranges::find(group, id) == group.end())
            group.push_back(id);
        return;
    }
    const auto [first, last] = std::minmax(anchor, target);
    for (auto it = first; it != last + 1; ++it)
        if (std::ranges::find(group, *it) == group.end())
            group.push_back(*it);
}

// Strg toggles `id`, Umschalt adds a range. An empty group starts with `seed`, the item chosen
// before.
template <class Id>
void toggleOrRange(std::vector<Id>& group, Id id, bool range, const std::vector<Id>& order,
                   std::optional<Id> seed) {
    if (group.empty() && seed && *seed != id)
        group.push_back(*seed);
    if (range)
        addRange(group, id, order);
    else
        toggle(group, id);
}

} // namespace

void MainWindow::createGroupSelection() {
    impl_->viewport->setBodyBoxSelectionHandler( // W07
        [this](const std::vector<std::uint64_t>& shown, bool additive) {
            selectBodiesInBox(shown, additive);
        });
    impl_->modelTree->setMenuHandler( // W09
        [this](ModelTreePanel::Kind kind, std::uint64_t id, QPoint at) {
            showTreeMenu(static_cast<int>(kind), id, at);
        });
    impl_->modelTree->setBodyGroupHandler(
        [this](std::uint64_t id, bool range) { groupBody(document::BodyId{id}, range); });
    impl_->timeline->setGroupHandler(
        [this](std::uint64_t id, bool range) { groupStep(document::FeatureId{id}, range); });
}

void MainWindow::groupBody(document::BodyId body, bool range) {
    if (impl_->shell().toolBlocksCommands())
        return;
    impl_->featureGroup.clear();
    toggleOrRange(impl_->bodyGroup, body, range, impl_->modelTree->bodyOrder(),
                  impl_->selectedBody);
    if (impl_->bodyGroup.empty())
        return clearGroups();
    // The last added body leads: the transform tool sits on it.
    selectBody(impl_->bodyGroup.back().value(), true);
    syncGroups();
}

// W07 (as in Fusion): a box over bodies outside a sketch selects them; several make a group
// (N02b), Ctrl adds to the selection, an empty box clears it.
void MainWindow::selectBodiesInBox(const std::vector<std::uint64_t>& shown, bool additive) {
    if (impl_->shell().toolBlocksCommands())
        return;
    const auto bodies = bodiesShown(impl_->session.displayBodies(), shown);
    if (!additive)
        clearGroups();
    if (bodies.empty()) {
        if (!additive)
            clearSelectionEverywhere();
        return;
    }
    if (bodies.size() == 1 && !additive)
        return selectBody(bodies.front().value());
    impl_->featureGroup.clear();
    extendGroup(impl_->bodyGroup, impl_->selectedBody, bodies, additive);
    selectBody(impl_->bodyGroup.back().value(), true);
    syncGroups();
}

void MainWindow::clearSelectionEverywhere() {
    impl_->selectedDatum.reset();
    impl_->selectedBody.reset();
    impl_->selectedPlanarFace.reset();
    impl_->session.clearSelection();
    syncModelTree();
    syncTimeline();
    syncProperties();
    syncShell();
}

void MainWindow::groupStep(document::FeatureId step, bool range) {
    if (impl_->shell().toolBlocksCommands())
        return;
    impl_->bodyGroup.clear();
    std::vector<document::FeatureId> order;
    for (const auto& feature : impl_->session.features())
        order.push_back(feature.id);
    toggleOrRange(impl_->featureGroup, step, range, order, impl_->session.selectedFeature());
    if (impl_->featureGroup.empty())
        return clearGroups();
    selectFeature(impl_->featureGroup.back().value(), true);
    syncGroups();
}

void MainWindow::clearGroups() {
    if (impl_->bodyGroup.empty() && impl_->featureGroup.empty())
        return;
    impl_->bodyGroup.clear();
    impl_->featureGroup.clear();
    syncGroups();
}

void MainWindow::syncGroups() {
    impl_->modelTree->markBodies(impl_->bodyGroup);
    impl_->timeline->markGroup(impl_->featureGroup);
    std::vector<std::uint64_t> companions;
    for (const auto body : impl_->bodyGroup)
        if (body != impl_->selectedBody)
            if (const auto shown =
                    impl_->session.transformBodyForFeature(document::FeatureId{body.value()}))
                companions.push_back(shown->value());
    impl_->viewport->setTransformCompanions(std::move(companions));
    if (impl_->bodyGroup.size() > 1)
        statusBar()->showMessage(
            tr("%n Körper gewählt", nullptr, static_cast<int>(impl_->bodyGroup.size())));
    else if (impl_->featureGroup.size() > 1)
        statusBar()->showMessage(
            tr("%n Schritte gewählt", nullptr, static_cast<int>(impl_->featureGroup.size())));
    syncBodyCommands();
}

std::vector<document::BodyId> MainWindow::selectedBodies() const {
    if (impl_->bodyGroup.size() > 1)
        return impl_->bodyGroup;
    if (impl_->selectedBody && impl_->shell().selection == ShellSelection::body)
        return {*impl_->selectedBody};
    return {};
}

// The steps a history command acts on: the timeline group, else the features of the chosen
// bodies, else the selected feature.
std::vector<document::FeatureId> MainWindow::chosenSteps() const {
    if (impl_->featureGroup.size() > 1)
        return impl_->featureGroup;
    if (impl_->bodyGroup.size() > 1) {
        std::vector<document::FeatureId> steps;
        for (const auto& feature : impl_->session.features())
            if (const auto body = impl_->session.resultBody(feature.id);
                body && std::ranges::find(impl_->bodyGroup, *body) != impl_->bodyGroup.end())
                steps.push_back(feature.id);
        return steps;
    }
    if (const auto selected = impl_->session.selectedFeature())
        return {*selected};
    return {};
}

// N02b: a group of bodies is moved together (no turning; CadSession::moveBodies).
bool MainWindow::moveBodyGroup(double deltaX, double deltaY, double deltaZ) {
    if (impl_->bodyGroup.size() < 2)
        return false;
    std::vector<document::FeatureId> features;
    for (const auto body : impl_->bodyGroup)
        features.push_back(document::FeatureId{body.value()});
    const auto group = impl_->bodyGroup;
    applyParameterCommand(
        [this, features, deltaX, deltaY, deltaZ](application::ErrorDisposition disposition) {
            return impl_->session.moveBodies(features, {deltaX, deltaY, deltaZ}, disposition);
        });
    impl_->bodyGroup = group; // the group stays chosen after the move
    syncGroups();
    return true;
}

// N02b: suppresses the group, or reactivates it when all of it is suppressed.
void MainWindow::suppressGroup() {
    const auto states = impl_->session.featureStates();
    const bool suppress = std::ranges::any_of(impl_->featureGroup, [&](document::FeatureId id) {
        const auto state = std::ranges::find(states, id, &application::FeatureRuntimeState::id);
        return state == states.end() ||
               state->status != application::FeatureRuntimeStatus::suppressed;
    });
    const auto group = impl_->featureGroup;
    if (!runCommand([&] { return impl_->session.setFeaturesSuppressed(group, suppress); },
                    [this](const QString& message) { statusBar()->showMessage(message); }))
        return;
    refreshScene();
    statusBar()->showMessage(
        suppress ? tr("%n Schritte unterdrückt", nullptr, static_cast<int>(group.size()))
                 : tr("%n Schritte aktiviert", nullptr, static_cast<int>(group.size())),
        2000);
}

// N02b: the steps of the group (or of the grouped bodies), with what depends on them named
// once for all.
void MainWindow::deleteGroup() {
    const auto chosen = chosenSteps();
    application::DeletionPlan plan;
    const auto add = [](std::vector<document::FeatureId>& list, document::FeatureId id) {
        if (std::ranges::find(list, id) == list.end())
            list.push_back(id);
    };
    for (const auto id : chosen) {
        const auto single = impl_->session.deletionPlan(id);
        for (const auto dependent : single.dependents)
            if (std::ranges::find(chosen, dependent) == chosen.end())
                add(plan.dependents, dependent);
        for (const auto doomed : single.cascade)
            add(plan.cascade, doomed);
        add(plan.cascade, id);
        plan.onlyThisPossible = plan.onlyThisPossible && single.onlyThisPossible;
    }
    deleteSteps(chosen, plan,
                tr("Die %n gewählten Schritte", nullptr, static_cast<int>(chosen.size())));
}

} // namespace voelund::desktop
