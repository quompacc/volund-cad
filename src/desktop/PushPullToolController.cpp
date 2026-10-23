// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "PushPullToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>
#include <QTimer>

#include <exception>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {
QString guidance() {
    return QObject::tr("Pfeil ziehen oder Strecke eingeben. Positiv zieht die Fläche heraus, "
                       "negativ drückt sie in den Körper.");
}
} // namespace

PushPullToolController::PushPullToolController(application::CadSession& session,
                                               viewport::CadViewport& viewport,
                                               ToolPanelHost& panel,
                                               std::function<void()> sceneChanged,
                                               std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void PushPullToolController::begin(const viewport::PlanarFaceSelection& face) {
    // The viewport names a face by the body's latest result (its source feature).
    const document::FeatureId support{face.bodyId};
    try {
        face_ = session_.faceReference(support, face.selectedFace).origin;
    } catch (const std::exception&) {
        statusChanged_(QObject::tr("Diese Fläche lässt sich nicht drücken oder ziehen. Wähle "
                                   "eine ebene Fläche eines Körpers."));
        return;
    }
    active_ = true;
    support_ = support;
    stamp_ = session_.sceneStatus().document;
    // The grip is shared with the fillet and chamfer panel; the open tool owns it.
    viewport_.setPushPullHandler(
        [this](double distance, bool finished) { dragged(distance, finished); });
    distance_ = 10.0;
    panel_.begin(withFieldBindings(ToolPanelDefinition{QObject::tr("Drücken/Ziehen"),
                                                       guidance(),
                                                       {{"distance", QObject::tr("Strecke"),
                                                         distance_, -1'000'000.0, 1'000'000.0}},
                                                       {},
                                                       {},
                                                       [this](const std::string& id, double value) {
                                                           if (id == "distance")
                                                               typed(value);
                                                       },
                                                       {},
                                                       {},
                                                       [this]() { accept(); },
                                                       [this]() { cancel(); },
                                                       {}},
                                   std::nullopt,
                                   [this] { stamp_ = session_.sceneStatus().document; }));
    panel_.focusLength("distance"); // W05: the main value takes the keyboard
    viewport_.beginPushPull(face);
    viewport_.setPushPullDistance(distance_);
    preview();
    statusChanged_(QObject::tr("Drücken/Ziehen: Pfeil ziehen oder Strecke eingeben"));
}

void PushPullToolController::dragged(double distance, bool finished) {
    if (!active_)
        return;
    distance_ = distance;
    panel_.setLengthValue("distance", distance);
    if (finished) {
        preview();
        return;
    }
    // S14 (user report 2026-10-10): the body follows the arrow while dragging, not only at
    // the release.
    if (previewPending_)
        return;
    previewPending_ = true;
    QTimer::singleShot(40, &panel_, [this] {
        previewPending_ = false;
        preview();
    });
}

void PushPullToolController::typed(double distance) {
    distance_ = distance;
    viewport_.setPushPullDistance(distance);
    preview();
}

void PushPullToolController::preview() {
    if (!active_ || !face_)
        return;
    if (distance_ == 0.0) {
        panel_.setAcceptEnabled(false);
        panel_.setGuidance(guidance());
        return;
    }
    const auto request = application::prepareRequest(
        session_, application::PushPullFaceAction{support_, *face_, millimetres(distance_)},
        stamp_);
    const auto candidate = application::previewAction(session_, request);
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setAcceptEnabled(ok);
    panel_.setGuidance(ok ? guidance() : commandFeedbackMessage(result));
    if (!ok)
        return;
    std::vector<viewport::SceneBody> bodies;
    for (const auto& body : candidate.bodies)
        bodies.push_back({body.sourceFeature.value(), body.shape, false, false});
    viewport_.setBodies(bodies);
}

void PushPullToolController::accept() {
    if (!face_)
        return;
    if (!runCommand(
            [this] {
                return runAction(
                    session_,
                    application::PushPullFaceAction{support_, *face_, millimetres(distance_)},
                    stamp_);
            },
            [this](const QString& message) {
                panel_.setGuidance(message);
                statusChanged_(message);
            }))
        return;
    finish();
    statusChanged_(distance_ > 0 ? QObject::tr("Fläche herausgezogen")
                                 : QObject::tr("Fläche hineingedrückt"));
}

void PushPullToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void PushPullToolController::finish() {
    active_ = false;
    face_.reset();
    viewport_.endPushPull();
    panel_.end();
    // The scene shows the model again instead of the preview.
    sceneChanged_();
}

} // namespace voelund::desktop
