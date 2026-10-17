#include "ShellToolController.hpp"

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

ShellToolController::ShellToolController(application::CadSession& session, ToolPanelHost& panel,
                                         std::function<void()> sceneChanged,
                                         std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void ShellToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

// A face of the model, or of the previewed body (named there like on the body before it).
std::optional<document::SubshapeOrigin>
ShellToolController::faceOrigin(const viewport::PlanarFaceSelection& face) const {
    const document::FeatureId support{face.bodyId};
    // Only faces of the shell's own body: its result before, the edited shell or the preview.
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

void ShellToolController::begin(const viewport::PlanarFaceSelection& face) {
    const auto origin = faceOrigin(face);
    if (!origin) {
        statusChanged_(QObject::tr("Wähle eine ebene Fläche des Körpers, die offen bleiben soll."));
        return;
    }
    support_ = document::FeatureId{face.bodyId};
    faces_ = {*origin};
    thickness_ = 2.0;
    outward_ = false;
    editing_.reset();
    stored_.reset();
    open();
}

void ShellToolController::beginEdit(document::FeatureId shell) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.shellParameters(shell);
    const auto producer = features::bodyProducerBefore(snapshot, shell, values.body);
    if (!producer || !session_.canAddCylinder(*producer))
        return;
    support_ = *producer;
    faces_ = values.openFaces;
    thickness_ = values.thickness;
    outward_ = values.outward;
    editing_ = shell;
    stored_ = values;
    open();
}

void ShellToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Wandstärke bearbeiten") : QObject::tr("Wandstärke"),
        guidance(),
        {{"thickness", QObject::tr("Dicke"), thickness_, 0.001, 1'000'000.0}},
        {{"direction",
          QObject::tr("Wände"),
          {QObject::tr("Nach innen"), QObject::tr("Nach außen")},
          outward_ ? 1 : 0}},
        {{"reset_faces", QObject::tr("Offene Flächen zurücksetzen"), QObject::tr("Wandstärke")}},
        [this](const std::string& id, double value) {
            if (id == "thickness")
                thickness_ = value;
            preview();
        },
        [this](const std::string& id, int index) {
            if (id == "direction")
                outward_ = index == 1;
            preview();
        },
        [this](const std::string& id) {
            if (id != "reset_faces")
                return;
            faces_.clear();
            preview();
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    preview();
    statusChanged_(QObject::tr("Wandstärke: weitere offene Flächen anklicken"));
}

void ShellToolController::picked(const viewport::PlanarFaceSelection& face) {
    if (!active_)
        return;
    const auto origin = faceOrigin(face);
    if (!origin) {
        const auto message = QObject::tr("Diese Fläche gehört nicht zum Körper der Wandstärke.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    const auto known = std::ranges::find(faces_, *origin);
    if (known != faces_.end())
        faces_.erase(known);
    else
        faces_.push_back(*origin);
    preview();
}

QString ShellToolController::guidance() const {
    if (faces_.empty())
        return QObject::tr("Klicke die Flächen an, die offen bleiben.");
    return QObject::tr("%1 offene Fläche(n), Wände %2 mm %3. Weitere Flächen anklicken.")
        .arg(faces_.size())
        .arg(thickness_)
        .arg(outward_ ? QObject::tr("nach außen") : QObject::tr("nach innen"));
}

void ShellToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

application::Action ShellToolController::action() const {
    if (editing_ && stored_)
        return application::EditShellParametersAction{
            *editing_, document::ShellParameters{stored_->body, faces_, thickness_, outward_}};
    return application::CreateShellAction{support_, faces_, millimetres(thickness_), outward_};
}

bool ShellToolController::unchanged() const {
    return stored_ &&
           *stored_ == document::ShellParameters{stored_->body, faces_, thickness_, outward_};
}

void ShellToolController::preview() {
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

void ShellToolController::accept() {
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
    statusChanged_(edited ? QObject::tr("Wandstärke geändert")
                          : QObject::tr("Wandstärke angelegt"));
}

void ShellToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void ShellToolController::finish() {
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
