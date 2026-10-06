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
        {{"distance", QObject::tr("Distanz"), distance_, 0.001, 1'000'000.0}},
        std::move(choices),
        {},
        [this](const std::string& id, double value) {
            if (id == "distance")
                setDistance(value);
        },
        [this](const std::string& id, int index) { choose(id, index); },
        {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    panel_.setAcceptEnabled(true);
    statusChanged_(QObject::tr("Extrusion bereit: %1 Bereich(e)").arg(profileIds_.size()));
}

void SketchExtrusionToolController::setDistance(double distance) { distance_ = distance; }

void SketchExtrusionToolController::choose(const std::string& choice, int index) {
    if (choice == "direction") {
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
            [this] {
                const bool named =
                    baseSketch_ && operation_ != document::ExtrusionOperation::newBody;
                return runAction(session_,
                                 application::CreateProfileExtrusionAction{
                                     *profileSketch_, profileIds_, millimetres(distance_),
                                     {operation_, reversed_, named ? target_ : std::nullopt}},
                                 supportStamp_);
            },
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
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

bool SketchExtrusionToolController::isActive() const noexcept { return active_; }

} // namespace voelund::desktop
