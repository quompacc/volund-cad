#include "SketchExtrusionToolController.hpp"
#include "CommandFeedback.hpp"

#include "ToolPanelHost.hpp"

#include "ShellActions.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QObject>

#include <algorithm>
#include <utility>

namespace voelund::desktop {

SketchExtrusionToolController::SketchExtrusionToolController(
    application::CadSession& session, ToolPanelHost& panel, std::function<void()> sceneChanged,
    std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchExtrusionToolController::beginSelectedProfiles() {
    if (!session_.canExtrudeSelectedSketchProfiles())
        return;
    active_ = true;
    supportStamp_ = session_.sceneStatus().document;
    profileSketch_ = session_.selectedFeature();
    const auto selected = session_.selectedSketchProfiles();
    profileIds_.assign(selected.begin(), selected.end());
    distance_ = 10.0;
    // T02 defaults: a face sketch joins its body, a base-plane sketch makes a new one.
    const auto features = session_.features();
    const auto sketch = std::ranges::find(features, *profileSketch_, &document::FeatureDescriptor::id);
    baseSketch_ = sketch != features.end() && !sketch->inputFeature;
    operation_ = baseSketch_ ? document::ExtrusionOperation::newBody
                             : document::ExtrusionOperation::join;
    reversed_ = false;
    offset_ = 0.0;
    upToFace_ = false;
    endFace_.reset();
    // A base-plane sketch joins or cuts a named body; the field shows when there is a choice.
    targets_.clear();
    std::vector<QString> targetNames;
    int number = 1;
    for (const auto& body : session_.modelOutline().bodies) {
        const int bodyNumber = number++; // Numbered as in the model tree.
        if (!baseSketch_ || std::ranges::find(session_.validBodies(), body.id,
                                              &features::EvaluatedBody::bodyId) ==
                                session_.validBodies().end())
            continue;
        targets_.push_back(body.id);
        targetNames.push_back(QObject::tr("Körper %1").arg(bodyNumber));
    }
    target_ = targets_.empty() ? std::nullopt : std::optional{targets_.front()};
    std::vector<ToolChoiceParameter> choices{
        {"end", QObject::tr("Ende"), {QObject::tr("Distanz"), QObject::tr("Bis Fläche")}, 0},
        {"direction",
         QObject::tr("Richtung"),
         {QObject::tr("Entlang Normale"), QObject::tr("Entgegen Normale")},
         0},
        {"operation",
         QObject::tr("Ergebnis"),
         {QObject::tr("Neuer Körper"), QObject::tr("Vereinigen"), QObject::tr("Abziehen")},
         static_cast<int>(operation_) - 1}};
    if (targets_.size() > 1)
        choices.push_back({"target", QObject::tr("Zielkörper"), targetNames, 0});
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Extrudieren"),
        QObject::tr("%1 gewählte(n) Skizzenbereich(e) extrudieren.").arg(profileIds_.size()),
        {{"distance", QObject::tr("Distanz"), distance_, 0.001, 1'000'000.0},
         {"offset", QObject::tr("Versatz"), offset_, -1'000'000.0, 1'000'000.0}},
        std::move(choices),
        {},
        [this](const std::string& id, double value) {
            if (id == "distance")
                setDistance(value);
            else if (id == "offset")
                offset_ = value;
            preview();
        },
        [this](const std::string& id, int index) {
            choose(id, index);
            preview();
        },
        {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    panel_.setAcceptEnabled(true);
    preview();
    statusChanged_(QObject::tr("Extrusion bereit: %1 Bereich(e)").arg(profileIds_.size()));
}

void SketchExtrusionToolController::setDistance(double distance) { distance_ = distance; }

void SketchExtrusionToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

application::Action SketchExtrusionToolController::extrusion() const {
    const bool named = baseSketch_ && operation_ != document::ExtrusionOperation::newBody;
    return application::CreateProfileExtrusionAction{
        *profileSketch_, profileIds_, millimetres(distance_),
        {operation_, reversed_, named ? target_ : std::nullopt, offset_,
         upToFace_ ? endFace_ : std::nullopt}};
}

// U11 (A-UI-02, A-VIS-01): the extrusion the values would give, on an isolated copy; the
// document stays unchanged until OK. An invalid combination says why and disables OK.
void SketchExtrusionToolController::preview() {
    if (!active_ || !profileSketch_ || !previewChanged_)
        return;
    // Without a valid candidate the view shows the model itself (faces stay pickable).
    const auto showModel = [this] {
        if (previewShown_)
            previewChanged_(nullptr);
        previewShown_ = false;
    };
    if (upToFace_ && !endFace_) {
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, extrusion(), supportStamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setAcceptEnabled(ok);
    if (!ok) {
        panel_.setGuidance(commandFeedbackMessage(result));
        showModel();
        return;
    }
    previewChanged_(&candidate.bodies);
    previewShown_ = true;
}

void SketchExtrusionToolController::choose(const std::string& choice, int index) {
    if (choice == "end") {
        upToFace_ = index == 1;
        panel_.setLengthEnabled("distance", !upToFace_);
        panel_.setAcceptEnabled(!upToFace_ || endFace_.has_value());
        panel_.setGuidance(upToFace_ && !endFace_
                               ? QObject::tr("Wähle im Modell die Fläche, an der die Extrusion endet.")
                               : QObject::tr("%1 gewählte(n) Skizzenbereich(e) extrudieren.")
                                     .arg(profileIds_.size()));
    } else if (choice == "direction") {
        reversed_ = index == 1;
    } else if (choice == "operation") {
        operation_ = static_cast<document::ExtrusionOperation>(index + 1);
        // A cut goes into the material; the panel shows the changed direction.
        if (operation_ == document::ExtrusionOperation::cut && !reversed_)
            panel_.chooseOption("direction", 1);
    } else if (choice == "target" && index >= 0 &&
               static_cast<std::size_t>(index) < targets_.size()) {
        target_ = targets_[static_cast<std::size_t>(index)];
    }
}

void SketchExtrusionToolController::accept() {
    if (supportStamp_ && *supportStamp_ != session_.sceneStatus().document) {
        const auto message = QObject::tr(
            "Das Modell hat sich geändert. Öffne die Extrusion erneut auf ihrer aktuellen Stütze.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (!profileSketch_.has_value())
        return;
    if (!runCommand(
            [this] { return runAction(session_, extrusion(), supportStamp_); },
            [this](const QString& message) {
                panel_.setGuidance(message);
                statusChanged_(message);
            }))
        return;
    panel_.end();
    profileSketch_.reset();
    profileIds_.clear();
    active_ = false;
    sceneChanged_();
    statusChanged_(QObject::tr("Gewählte Bereiche extrudiert"));
}

void SketchExtrusionToolController::cancel() {
    panel_.end();
    profileSketch_.reset();
    profileIds_.clear();
    active_ = false;
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr); // the model again instead of the preview
    previewShown_ = false;
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

bool SketchExtrusionToolController::isActive() const noexcept { return active_; }

bool SketchExtrusionToolController::awaitsEndFace() const noexcept { return active_ && upToFace_; }

void SketchExtrusionToolController::pickEndFace(document::FeatureId support,
                                                const TopoDS_Shape& face) {
    try {
        endFace_ = session_.faceReference(support, face);
    } catch (const std::exception&) {
        panel_.setGuidance(QObject::tr("Diese Fläche lässt sich nicht als Ende verwenden. Wähle "
                                       "eine ebene Fläche eines Körpers."));
        return;
    }
    panel_.setAcceptEnabled(true);
    panel_.setGuidance(QObject::tr("Endfläche gewählt. Eine andere Fläche ersetzt sie."));
    preview();
    statusChanged_(QObject::tr("Endfläche gewählt"));
}

} // namespace voelund::desktop
