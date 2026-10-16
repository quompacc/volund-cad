#include "MirrorToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
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

MirrorToolController::MirrorToolController(application::CadSession& session, ToolPanelHost& panel,
                                           std::function<void()> sceneChanged,
                                           std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void MirrorToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// A planar face of the model. On a joined preview the original's faces count too; a face of
// the image itself cannot be its own plane.
std::optional<MirrorToolController::PickedFace>
MirrorToolController::faceOf(const viewport::PlanarFaceSelection& face) const {
    const document::FeatureId id{face.bodyId};
    const auto ownImage = [&](const document::BodyFaceReference& reference) {
        return std::ranges::any_of(reference.origin.instances, [&](const auto& instance) {
            return instance.feature == id || instance.feature == editing_;
        });
    };
    try {
        auto reference = session_.faceReference(id, face.selectedFace);
        if (!ownImage(reference))
            return PickedFace{id, std::move(reference)};
        return std::nullopt;
    } catch (const std::exception&) {
    }
    if (previewSession_ && previewFeature_ && id == *previewFeature_) {
        try {
            auto reference = previewSession_->faceReference(id, face.selectedFace);
            if (!ownImage(reference))
                return PickedFace{support_, std::move(reference)};
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}

void MirrorToolController::begin(document::FeatureId support,
                                 const std::optional<viewport::PlanarFaceSelection>& face) {
    support_ = support;
    editing_.reset();
    stored_.reset();
    mode_ = document::MirrorMode::newBody;
    constructionPlanes_.clear();
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    for (const auto& feature : snapshot.features())
        if (feature.kind == document::FeatureKind::datumPlane)
            constructionPlanes_.push_back(feature);
    face_ = face ? faceOf(*face) : std::nullopt;
    planeIndex_ = face_ ? faceChoice() : 0;
    open();
}

void MirrorToolController::beginEdit(document::FeatureId mirror) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.mirrorParameters(mirror);
    const auto producer = features::bodyProducerBefore(snapshot, mirror, values.body);
    if (!producer)
        return;
    support_ = *producer;
    editing_ = mirror;
    stored_ = values;
    mode_ = values.mode;
    constructionPlanes_.clear();
    for (const auto& feature : snapshot.features()) {
        if (feature.id == mirror)
            break;
        if (feature.kind == document::FeatureKind::datumPlane)
            constructionPlanes_.push_back(feature);
    }
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

int MirrorToolController::faceChoice() const noexcept {
    return basePlaneCount + static_cast<int>(constructionPlanes_.size());
}

void MirrorToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    std::vector<QString> planes{QObject::tr("XY-Ebene"), QObject::tr("XZ-Ebene"),
                                QObject::tr("YZ-Ebene")};
    for (const auto& feature : constructionPlanes_)
        planes.push_back(QString::fromStdString(feature.name));
    planes.push_back(QObject::tr("Ebene Fläche (anklicken)"));
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Spiegeln bearbeiten") : QObject::tr("Spiegeln"),
        guidance(),
        {},
        {{"plane", QObject::tr("Spiegelebene"), planes, planeIndex_},
         {"mode", QObject::tr("Ergebnis"), {QObject::tr("Neuer Körper"), QObject::tr("Vereinigen")},
          mode_ == document::MirrorMode::join ? 1 : 0}},
        {},
        [](const std::string&, double) {},
        [this](const std::string& id, int index) {
            if (id == "plane")
                planeIndex_ = index;
            if (id == "mode")
                mode_ = index == 1 ? document::MirrorMode::join : document::MirrorMode::newBody;
            preview();
        },
        [](const std::string&) {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    preview();
    statusChanged_(QObject::tr("Spiegeln: Ebene wählen oder eine ebene Fläche anklicken"));
}

void MirrorToolController::picked(const viewport::PlanarFaceSelection& face) {
    if (!active_)
        return;
    auto picked = faceOf(face);
    if (!picked) {
        const auto message =
            QObject::tr("Diese Fläche kann nicht Spiegelebene sein; wähle eine ebene Fläche des "
                        "Modells.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    face_ = std::move(picked);
    panel_.chooseOption("plane", faceChoice()); // previews through choiceChanged
}

std::optional<document::MirrorPlane> MirrorToolController::plane() const {
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

QString MirrorToolController::guidance() const {
    if (!plane())
        return QObject::tr("Klicke eine ebene Fläche an, an der gespiegelt wird.");
    return mode_ == document::MirrorMode::join
               ? QObject::tr("Das Spiegelbild wird mit dem Körper vereinigt.")
               : QObject::tr("Das Spiegelbild wird ein eigener Körper; das Original bleibt.");
}

void MirrorToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

application::Action MirrorToolController::action() const {
    const auto chosen = *plane();
    if (editing_ && stored_)
        return application::EditMirrorParametersAction{
            *editing_, document::MirrorParameters{stored_->body, chosen, mode_}};
    return application::CreateMirrorAction{
        support_,
        chosen.basePlane,
        chosen.constructionPlane,
        chosen.face ? std::optional{face_->support} : std::nullopt,
        chosen.face ? std::optional{chosen.face->origin} : std::nullopt,
        mode_};
}

bool MirrorToolController::unchanged() const {
    const auto chosen = plane();
    return stored_ && chosen &&
           *stored_ == document::MirrorParameters{stored_->body, *chosen, mode_};
}

void MirrorToolController::preview() {
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
    panel_.setErrorCommitHandler({});
    panel_.setAcceptEnabled(ok);
    if (!ok) {
        panel_.setGuidance(commandFeedbackMessage(result));
        offerKeepingSuccessorErrors(panel_, result, editing_, [this] {
            keep_ = application::ErrorDisposition::keepEditableErrors;
            accept();
            keep_.reset();
        });
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

void MirrorToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (!plane())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_, keep_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Spiegeln geändert") : QObject::tr("Körper gespiegelt"));
}

void MirrorToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void MirrorToolController::finish() {
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
