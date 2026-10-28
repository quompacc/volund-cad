// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "RibToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <algorithm>
#include <utility>

namespace voelund::desktop {

RibToolController::RibToolController(application::CadSession& session, ToolPanelHost& panel,
                                     std::function<void()> sceneChanged,
                                     std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void RibToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// The bodies a rib can join, named as the browser names them.
void RibToolController::collectBodies() {
    bodies_.clear();
    int number = 0;
    for (const auto& body : session_.modelOutline().bodies) {
        ++number;
        if (body.consumed)
            continue;
        bodies_.push_back({body.name.empty() ? QObject::tr("Körper %1").arg(number)
                                             : QString::fromStdString(body.name),
                           body.id});
    }
}

void RibToolController::begin(document::FeatureId sketch) {
    sketch_ = sketch;
    editing_.reset();
    stored_.reset();
    rib_ = {};
    collectBodies();
    bodyIndex_ = 0;
    open();
}

void RibToolController::beginEdit(document::FeatureId rib) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, rib, &document::FeatureDescriptor::id);
    const auto& values = snapshot.extrusionParameters(rib);
    if (found == features.end() || !found->inputFeature || !values.rib || !values.target)
        return;
    sketch_ = *found->inputFeature;
    editing_ = rib;
    stored_ = values;
    rib_ = *values.rib;
    collectBodies();
    const auto body =
        std::ranges::find(bodies_, *values.target, &decltype(bodies_)::value_type::second);
    bodyIndex_ = body == bodies_.end() ? 0 : static_cast<int>(body - bodies_.begin());
    open();
}

void RibToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    std::vector<QString> names;
    for (const auto& body : bodies_)
        names.push_back(body.first);
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Rippe bearbeiten") : QObject::tr("Rippe"),
        QObject::tr("Die Rippe wächst von der Linie bis zu den Flächen des Körpers."),
        {{"thickness", QObject::tr("Dicke"), rib_.thickness, 0.001, 1'000'000.0}},
        {{"direction",
          QObject::tr("Richtung"),
          {QObject::tr("Normal"), QObject::tr("Umgekehrt")},
          rib_.flipped ? 1 : 0},
         {"body", QObject::tr("Körper"), names, bodyIndex_}},
        {},
        [this](const std::string& id, double value) {
            if (id == "thickness")
                rib_.thickness = value;
            preview();
        },
        [this](const std::string& id, int index) {
            if (id == "direction")
                rib_.flipped = index == 1;
            if (id == "body")
                bodyIndex_ = index;
            preview();
        },
        [](const std::string&) {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    panel_.focusLength("thickness");
    preview();
    statusChanged_(QObject::tr("Rippe: Dicke und Richtung wählen"));
}

application::Action RibToolController::action() const {
    const auto body = bodies_.at(static_cast<std::size_t>(bodyIndex_)).second;
    if (editing_ && stored_) {
        auto parameters = *stored_;
        parameters.rib = rib_;
        parameters.target = body;
        return application::EditExtrusionParametersAction{*editing_, parameters};
    }
    return application::CreateRibAction{sketch_, body, rib_};
}

bool RibToolController::unchanged() const {
    return stored_ && !bodies_.empty() && stored_->rib == rib_ &&
           stored_->target == bodies_.at(static_cast<std::size_t>(bodyIndex_)).second;
}

void RibToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
}

void RibToolController::preview() {
    if (!active_)
        return;
    if (bodies_.empty() || unchanged()) {
        panel_.setAcceptEnabled(!bodies_.empty());
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, action(), stamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setAcceptEnabled(ok);
    panel_.setGuidance(
        ok ? QObject::tr("Die Rippe wächst von der Linie bis zu den Flächen des Körpers.")
           : commandFeedbackMessage(result));
    if (!ok) {
        showModel();
        return;
    }
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

void RibToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (bodies_.empty())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Rippe geändert") : QObject::tr("Rippe angelegt"));
}

void RibToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void RibToolController::finish() {
    active_ = false;
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
