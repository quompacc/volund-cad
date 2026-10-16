#include "DraftToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>

#include <QObject>

#include <algorithm>
#include <exception>
#include <numbers>
#include <utility>

namespace voelund::desktop {

DraftToolController::DraftToolController(application::CadSession& session, ToolPanelHost& panel,
                                         std::function<void()> sceneChanged,
                                         std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void DraftToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// A face of the model, or of the previewed body (named there like on the body before it).
std::optional<document::SubshapeOrigin>
DraftToolController::faceOrigin(const viewport::PlanarFaceSelection& face) const {
    const document::FeatureId support{face.bodyId};
    // Only faces of the draft's own body: its result before, the edited draft or the preview.
    if (active_ && support != support_ && support != editing_ && support != previewFeature_)
        return std::nullopt;
    try {
        return session_.faceReference(support, face.selectedFace).origin;
    } catch (const std::exception&) {
    }
    if (previewSession_ && previewFeature_ && support == *previewFeature_) {
        try {
            return previewSession_->faceReference(support, face.selectedFace).origin;
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}

void DraftToolController::begin(const viewport::PlanarFaceSelection& face) {
    const auto origin = faceOrigin(face);
    if (!origin) {
        statusChanged_(QObject::tr("Wähle die neutrale ebene Fläche des Körpers."));
        return;
    }
    support_ = document::FeatureId{face.bodyId};
    neutral_ = *origin;
    faces_.clear();
    angleDegrees_ = 5.0;
    pickNeutral_ = false;
    editing_.reset();
    stored_.reset();
    open();
}

void DraftToolController::beginEdit(document::FeatureId draft) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.draftParameters(draft);
    const auto producer = features::bodyProducerBefore(snapshot, draft, values.body);
    if (!producer || !session_.canAddCylinder(*producer))
        return;
    support_ = *producer;
    faces_ = values.faces;
    neutral_ = values.neutral;
    angleDegrees_ = values.angle.value() * 180.0 / std::numbers::pi;
    pickNeutral_ = false;
    editing_ = draft;
    stored_ = values;
    open();
}

void DraftToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Formschräge bearbeiten") : QObject::tr("Formschräge"),
        guidance(),
        {{"angle", QObject::tr("Winkel"), angleDegrees_, -89.0, 89.0, ToolValueKind::angle}},
        {},
        {{"reset_faces", QObject::tr("Geneigte Flächen zurücksetzen"), QObject::tr("Formschräge")},
         {"pick_neutral", QObject::tr("Neutrale Fläche neu wählen"), QObject::tr("Formschräge")}},
        [this](const std::string& id, double value) {
            if (id == "angle")
                angleDegrees_ = value;
            preview();
        },
        {},
        [this](const std::string& id) {
            if (id == "reset_faces")
                faces_.clear();
            else if (id == "pick_neutral")
                pickNeutral_ = true;
            preview();
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    preview();
    statusChanged_(QObject::tr("Formschräge: zu neigende Flächen anklicken"));
}

void DraftToolController::picked(const viewport::PlanarFaceSelection& face) {
    if (!active_)
        return;
    const auto origin = faceOrigin(face);
    if (!origin) {
        const auto message = QObject::tr("Diese Fläche gehört nicht zum Körper der Formschräge.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (pickNeutral_) {
        pickNeutral_ = false;
        neutral_ = *origin;
        std::erase(faces_, *origin);
        preview();
        return;
    }
    if (*origin == neutral_)
        return; // the neutral face stays as it is
    const auto known = std::ranges::find(faces_, *origin);
    if (known != faces_.end())
        faces_.erase(known);
    else
        faces_.push_back(*origin);
    preview();
}

QString DraftToolController::guidance() const {
    if (pickNeutral_)
        return QObject::tr("Klicke die neue neutrale Fläche an.");
    if (faces_.empty())
        return QObject::tr("Klicke die Flächen an, die geneigt werden.");
    return QObject::tr("%1 Fläche(n) um %2° geneigt. Weitere Flächen anklicken.")
        .arg(faces_.size())
        .arg(angleDegrees_);
}

void DraftToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

document::DraftParameters DraftToolController::parameters(document::BodyId body) const {
    return {body, faces_, *neutral_, kernel::Radians{angleDegrees_ * std::numbers::pi / 180.0}};
}

application::Action DraftToolController::action() const {
    if (editing_ && stored_)
        return application::EditDraftParametersAction{*editing_, parameters(stored_->body)};
    return application::CreateDraftAction{
        support_, faces_, *neutral_,
        application::Quantity{angleDegrees_ * std::numbers::pi / 180.0,
                              application::QuantityUnit::radian}};
}

bool DraftToolController::unchanged() const {
    return stored_ && *stored_ == parameters(stored_->body);
}

void DraftToolController::preview() {
    if (!active_)
        return;
    if (faces_.empty() || unchanged()) {
        panel_.setAcceptEnabled(!faces_.empty());
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

void DraftToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (faces_.empty())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_, keep_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Formschräge geändert")
                          : QObject::tr("Formschräge angelegt"));
}

void DraftToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void DraftToolController::finish() {
    active_ = false;
    faces_.clear();
    neutral_.reset();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
