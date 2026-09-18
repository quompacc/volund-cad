#include "SketchExtrusionToolController.hpp"
#include "CommandFeedback.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QObject>

#include <utility>

namespace voelund::desktop {

SketchExtrusionToolController::SketchExtrusionToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchExtrusionToolController::begin(const viewport::PlanarFaceSelection& support) {
    try {
        const document::SketchPlane plane{
            support.originX,     support.originY,     support.originZ,
            support.normalX,     support.normalY,     support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
        supportBinding_ = session_.bindSelectedSupport(document::FeatureId{support.bodyId},
                                                       support.selectedFace, plane);
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    supportStamp_ = session_.sceneStatus().document;
    active_ = true;
    result_.reset();
    profileSketch_.reset();
    profileIds_.clear();
    distance_ = 1.0;
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Sketch / Extrusion"),
        QObject::tr("Set two rectangle corners, pull the preview, then confirm."),
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
    viewport_.beginRectangleSketch(support);
    statusChanged_(QObject::tr("Sketch: set two rectangle corners"));
}

void SketchExtrusionToolController::beginSelectedProfiles() {
    if (!session_.canExtrudeSelectedSketchProfiles())
        return;
    active_ = true;
    supportBinding_.reset();
    supportStamp_ = session_.sceneStatus().document;
    result_.reset();
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

void SketchExtrusionToolController::updateDistancePreview(double distance) {
    distance_ = distance;
    panel_.setLengthValue("distance", distance);
    statusChanged_(QObject::tr("Extrusion: %1 mm").arg(distance, 0, 'f', 3));
}

void SketchExtrusionToolController::setResult(const viewport::SketchRectangleResult& result) {
    result_ = result;
    distance_ = result.distance;
    panel_.setLengthValue("distance", distance_);
    panel_.setAcceptEnabled(true);
    statusChanged_(QObject::tr("Preview ready — confirm with OK or cancel"));
}

void SketchExtrusionToolController::setDistance(double distance) {
    distance_ = distance;
    if (result_.has_value()) {
        viewport_.updateSketchExtrusionPreview(distance);
        result_->distance = distance;
    }
}

void SketchExtrusionToolController::accept() {
    if (supportStamp_ && *supportStamp_ != session_.sceneStatus().document) {
        const auto message =
            QObject::tr("The model changed. Reopen the extrusion on its current support.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    if (profileSketch_.has_value()) {
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
        return;
    }
    if (!result_.has_value()) {
        return;
    }
    const viewport::PlanarFaceSelection& support = result_->support;
    const document::SketchPlane plane{
        support.originX,     support.originY,     support.originZ,
        support.normalX,     support.normalY,     support.normalZ,
        support.xDirectionX, support.xDirectionY, support.xDirectionZ};
    const document::SketchRectangle rectangle{result_->firstX, result_->firstY, result_->secondX,
                                              result_->secondY};
    if (!runCommand(
            [&] {
                document::SketchParameters sketch{plane,
                                                  {{{document::SketchElementId{1}, rectangle}}, 2}};
                sketch.supportBinding = supportBinding_;
                return session_.createSketchExtrusion(document::FeatureId{support.bodyId}, sketch,
                                                      distance_);
            },
            [this](const QString& message) {
                panel_.setGuidance(message);
                statusChanged_(message);
            }))
        return;
    viewport_.cancelSketch();
    panel_.end();
    result_.reset();
    profileSketch_.reset();
    profileIds_.clear();
    profileSketch_.reset();
    profileIds_.clear();
    active_ = false;
    sceneChanged_();
    statusChanged_(QObject::tr("Sketch and extrusion created"));
}

void SketchExtrusionToolController::cancel() {
    viewport_.cancelSketch();
    panel_.end();
    result_.reset();
    active_ = false;
    statusChanged_(QObject::tr("Tool cancelled"));
}

bool SketchExtrusionToolController::isActive() const noexcept { return active_; }

} // namespace voelund::desktop
