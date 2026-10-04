#include "SketchExtrusionToolController.hpp"
#include "CommandFeedback.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QObject>

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
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Extrude Profiles"),
        QObject::tr("Extrude %1 selected sketch profile(s).").arg(profileIds_.size()),
        {{"distance", QObject::tr("Distance"), distance_, 0.001, 1'000'000.0}},
        {},
        {},
        [this](const std::string& id, double value) {
            if (id == "distance")
                setDistance(value);
        },
        {},
        {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    panel_.setAcceptEnabled(true);
    statusChanged_(QObject::tr("Extrusion ready: %1 profile(s)").arg(profileIds_.size()));
}

void SketchExtrusionToolController::setDistance(double distance) { distance_ = distance; }

void SketchExtrusionToolController::accept() {
    if (supportStamp_ && *supportStamp_ != session_.sceneStatus().document) {
        const auto message =
            QObject::tr("The model changed. Reopen the extrusion on its current support.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (!profileSketch_.has_value())
        return;
    if (!runCommand(
            [this] {
                return session_.createSketchProfileExtrusion(*profileSketch_, profileIds_,
                                                             distance_);
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
    statusChanged_(QObject::tr("Selected profiles extruded"));
}

void SketchExtrusionToolController::cancel() {
    panel_.end();
    profileSketch_.reset();
    profileIds_.clear();
    active_ = false;
    statusChanged_(QObject::tr("Tool cancelled"));
}

bool SketchExtrusionToolController::isActive() const noexcept { return active_; }

} // namespace voelund::desktop
