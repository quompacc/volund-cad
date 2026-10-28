// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SplitToolController.hpp"

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
namespace {
constexpr int basePlaneCount = 3;
} // namespace

SplitToolController::SplitToolController(application::CadSession& session, ToolPanelHost& panel,
                                         std::function<void()> sceneChanged,
                                         std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SplitToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// A planar face of the model; on the preview the faces of the split body count as its own.
std::optional<SplitToolController::PickedFace>
SplitToolController::faceOf(const viewport::PlanarFaceSelection& face) const {
    const document::FeatureId id{face.bodyId};
    try {
        return PickedFace{id, session_.faceReference(id, face.selectedFace)};
    } catch (const std::exception&) {
    }
    if (previewSession_ && previewFeature_ && id == *previewFeature_) {
        try {
            return PickedFace{support_, previewSession_->faceReference(id, face.selectedFace)};
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}

void SplitToolController::collectPlanes(std::optional<document::FeatureId> before) {
    constructionPlanes_.clear();
    for (const auto& feature : session_.features()) {
        if (before && feature.id == *before)
            break;
        if (feature.kind == document::FeatureKind::datumPlane)
            constructionPlanes_.push_back(feature);
    }
}

void SplitToolController::begin(document::FeatureId support,
                                const std::optional<viewport::PlanarFaceSelection>& face) {
    support_ = support;
    editing_.reset();
    stored_.reset();
    keep_ = document::SplitKeep::both;
    collectPlanes(std::nullopt);
    face_ = face ? faceOf(*face) : std::nullopt;
    // A construction plane is the usual cutting plane; else the picked face or YZ.
    planeIndex_ = face_ ? faceChoice() : constructionPlanes_.empty() ? 2 : faceChoice() - 1;
    open();
}

void SplitToolController::beginEdit(document::FeatureId split) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.splitParameters(split);
    const auto producer = features::bodyProducerBefore(snapshot, split, values.body);
    if (!producer)
        return;
    support_ = *producer;
    editing_ = split;
    stored_ = values;
    keep_ = values.keep;
    collectPlanes(split);
    face_.reset();
    if (values.plane.basePlane) {
        planeIndex_ = static_cast<int>(*values.plane.basePlane) - 1;
    } else if (values.plane.constructionPlane) {
        const auto found = std::ranges::find(constructionPlanes_, *values.plane.constructionPlane,
                                             &document::FeatureDescriptor::id);
        planeIndex_ = basePlaneCount + static_cast<int>(found - constructionPlanes_.begin());
    } else {
        face_ = PickedFace{*producer, *values.plane.face};
        planeIndex_ = faceChoice();
    }
    open();
}

int SplitToolController::faceChoice() const noexcept {
    return basePlaneCount + static_cast<int>(constructionPlanes_.size());
}

void SplitToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    std::vector<QString> planes{QObject::tr("XY-Ebene"), QObject::tr("XZ-Ebene"),
                                QObject::tr("YZ-Ebene")};
    for (const auto& feature : constructionPlanes_)
        planes.push_back(QString::fromStdString(feature.name));
    planes.push_back(QObject::tr("Ebene Fläche (anklicken)"));
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Teilen bearbeiten") : QObject::tr("Teilen"),
        guidance(),
        {},
        {{"plane", QObject::tr("Trennebene"), planes, planeIndex_},
         {"keep",
          QObject::tr("Behalten"),
          {QObject::tr("Beide Seiten"), QObject::tr("Vorderseite"), QObject::tr("Rückseite")},
          static_cast<int>(keep_) - 1}},
        {},
        [](const std::string&, double) {},
        [this](const std::string& id, int index) {
            if (id == "plane")
                planeIndex_ = index;
            if (id == "keep")
                keep_ = static_cast<document::SplitKeep>(index + 1);
            preview();
        },
        [](const std::string&) {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    preview();
    statusChanged_(QObject::tr("Teilen: Ebene wählen oder eine ebene Fläche anklicken"));
}

void SplitToolController::picked(const viewport::PlanarFaceSelection& face) {
    if (!active_)
        return;
    auto picked = faceOf(face);
    if (!picked) {
        const auto message =
            QObject::tr("Diese Fläche kann nicht Trennebene sein; wähle eine ebene Fläche des "
                        "Modells.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    face_ = std::move(picked);
    panel_.chooseOption("plane", faceChoice()); // previews through choiceChanged
}

std::optional<document::MirrorPlane> SplitToolController::plane() const {
    document::MirrorPlane result;
    if (planeIndex_ < basePlaneCount)
        result.basePlane = static_cast<document::DatumPlane>(planeIndex_ + 1);
    else if (planeIndex_ < faceChoice())
        result.constructionPlane =
            constructionPlanes_[static_cast<std::size_t>(planeIndex_ - basePlaneCount)].id;
    else if (face_)
        result.face = face_->reference;
    else
        return std::nullopt;
    return result;
}

QString SplitToolController::guidance() const {
    if (!plane())
        return QObject::tr("Klicke eine ebene Fläche an, an der geteilt wird.");
    switch (keep_) {
    case document::SplitKeep::front:
        return QObject::tr("Nur die Vorderseite (in Richtung der Ebenennormale) bleibt.");
    case document::SplitKeep::back:
        return QObject::tr("Nur die Rückseite bleibt.");
    default:
        return QObject::tr("Beide Seiten bleiben; die Rückseite wird ein eigener Körper.");
    }
}

void SplitToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

application::Action SplitToolController::action() const {
    const auto chosen = *plane();
    if (editing_ && stored_)
        return application::EditSplitParametersAction{
            *editing_, document::SplitParameters{stored_->body, chosen, keep_}};
    return application::CreateSplitAction{
        support_,
        chosen.basePlane,
        chosen.constructionPlane,
        chosen.face ? std::optional{face_->support} : std::nullopt,
        chosen.face ? std::optional{chosen.face->origin} : std::nullopt,
        keep_};
}

bool SplitToolController::unchanged() const {
    const auto chosen = plane();
    return stored_ && chosen &&
           *stored_ == document::SplitParameters{stored_->body, *chosen, keep_};
}

void SplitToolController::preview() {
    if (!active_)
        return;
    if (!plane() || unchanged()) {
        panel_.setAcceptEnabled(plane().has_value());
        panel_.setGuidance(guidance());
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
    panel_.setGuidance(guidance());
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

void SplitToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (!plane())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Teilen geändert") : QObject::tr("Körper geteilt"));
}

void SplitToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void SplitToolController::finish() {
    active_ = false;
    face_.reset();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
