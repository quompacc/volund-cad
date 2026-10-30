// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FaceEditToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>

#include <QObject>

#include <algorithm>
#include <exception>
#include <utility>

namespace voelund::desktop {

FaceEditToolController::FaceEditToolController(application::CadSession& session,
                                               ToolPanelHost& panel,
                                               std::function<void()> sceneChanged,
                                               std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void FaceEditToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// A face of the edited body: on its result before the edit, the edit itself or the preview.
std::optional<document::SubshapeOrigin>
FaceEditToolController::faceOrigin(std::uint64_t body, const TopoDS_Shape& face) const {
    const document::FeatureId support{body};
    if (active_ && support != support_ && support != editing_ && support != previewFeature_)
        return std::nullopt;
    if (auto origin = session_.faceOrigin(support, face))
        return origin;
    if (previewSession_ && previewFeature_ && support == *previewFeature_)
        return previewSession_->faceOrigin(support, face);
    return std::nullopt;
}

void FaceEditToolController::begin(std::uint64_t body, const TopoDS_Shape& face,
                                   document::FaceEditMode mode) {
    const auto origin = faceOrigin(body, face);
    if (!origin) {
        statusChanged_(QObject::tr("Wähle eine Fläche eines Körpers."));
        return;
    }
    support_ = document::FeatureId{body};
    faces_ = {*origin};
    mode_ = mode;
    distance_ = 1.0;
    editing_.reset();
    stored_.reset();
    open();
}

void FaceEditToolController::beginEdit(document::FeatureId edit) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.faceEditParameters(edit);
    const auto producer = features::bodyProducerBefore(snapshot, edit, values.body);
    if (!producer)
        return;
    support_ = *producer;
    faces_ = values.faces;
    mode_ = values.mode;
    distance_ = values.distance;
    editing_ = edit;
    stored_ = values;
    open();
}

void FaceEditToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(withFieldBindings(
        ToolPanelDefinition{
            editing_ ? QObject::tr("Flächen bearbeiten") : QObject::tr("Flächen"),
            guidance(),
            {{"distance", QObject::tr("Abstand"), distance_, -1'000'000.0, 1'000'000.0}},
            {{"mode",
              QObject::tr("Art"),
              {QObject::tr("Versetzen"), QObject::tr("Löschen")},
              mode_ == document::FaceEditMode::remove ? 1 : 0}},
            {},
            [this](const std::string& id, double value) {
                if (id == "distance")
                    distance_ = value;
                preview();
            },
            [this](const std::string& id, int index) {
                if (id == "mode")
                    mode_ = index == 1 ? document::FaceEditMode::remove
                                       : document::FaceEditMode::offset;
                panel_.setLengthVisible("distance", mode_ == document::FaceEditMode::offset);
                preview();
            },
            [](const std::string&) {},
            [this]() { accept(); },
            [this]() { cancel(); },
            {}},
        editing_, [this] { stamp_ = session_.sceneStatus().document; }));
    panel_.setLengthVisible("distance", mode_ == document::FaceEditMode::offset);
    panel_.focusLength("distance");
    preview();
    statusChanged_(QObject::tr("Flächen: weitere Flächen anklicken"));
}

void FaceEditToolController::picked(std::uint64_t body, const TopoDS_Shape& face) {
    if (!active_)
        return;
    const auto origin = faceOrigin(body, face);
    if (!origin) {
        const auto message = QObject::tr("Diese Fläche gehört nicht zum bearbeiteten Körper.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (const auto known = std::ranges::find(faces_, *origin); known != faces_.end())
        faces_.erase(known);
    else
        faces_.push_back(*origin);
    preview();
}

QString FaceEditToolController::guidance() const {
    if (faces_.empty())
        return QObject::tr("Klicke die Flächen an, die bearbeitet werden.");
    if (mode_ == document::FaceEditMode::remove)
        return QObject::tr("%1 Fläche(n) werden gelöscht; die Flächen daneben schließen die "
                           "Lücke.")
            .arg(faces_.size());
    return QObject::tr("%1 Fläche(n) um %2 mm versetzt (negativ: nach innen).")
        .arg(faces_.size())
        .arg(distance_);
}

void FaceEditToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

document::FaceEditParameters FaceEditToolController::parameters() const {
    return {stored_ ? stored_->body : document::BodyId{support_.value()}, faces_, mode_, distance_};
}

application::Action FaceEditToolController::action() const {
    if (editing_ && stored_)
        return application::EditFaceEditParametersAction{*editing_, parameters()};
    return application::CreateFaceEditAction{support_, faces_, mode_, distance_};
}

bool FaceEditToolController::unchanged() const { return stored_ && *stored_ == parameters(); }

void FaceEditToolController::preview() {
    if (!active_)
        return;
    panel_.setGuidance(guidance());
    if (faces_.empty() || unchanged()) {
        panel_.setAcceptEnabled(!faces_.empty());
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, action(), stamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setAcceptEnabled(ok);
    if (!ok) {
        panel_.setGuidance(commandFeedbackMessage(result));
        showModel();
        return;
    }
    previewSession_ = candidate.candidate;
    previewFeature_ = editing_ ? editing_
                      : candidate.result.createdFeatures.empty()
                          ? std::nullopt
                          : std::optional{candidate.result.createdFeatures.front()};
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

void FaceEditToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (faces_.empty())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Flächen geändert") : QObject::tr("Flächen bearbeitet"));
}

void FaceEditToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void FaceEditToolController::finish() {
    active_ = false;
    faces_.clear();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
