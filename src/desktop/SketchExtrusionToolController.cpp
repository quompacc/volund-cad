#include "SketchExtrusionToolController.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QObject>

#include <utility>

namespace voelund::desktop {

SketchExtrusionToolController::SketchExtrusionToolController(
    application::CadSession& session,
    viewport::CadViewport& viewport,
    ToolPanelHost& panel,
    std::function<void()> sceneChanged,
    std::function<void(const QString&)> statusChanged)
    : session_(session)
    , viewport_(viewport)
    , panel_(panel)
    , sceneChanged_(std::move(sceneChanged))
    , statusChanged_(std::move(statusChanged))
{
}

void SketchExtrusionToolController::begin(
    const viewport::PlanarFaceSelection& support)
{
    active_ = true;
    result_.reset();
    distance_ = 1.0;
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Sketch / Extrusion"),
        QObject::tr("Set two rectangle corners, pull the preview, then confirm."),
        {{"distance", QObject::tr("Distance"), distance_, 0.001, 1'000'000.0}},
        {},
        {},
        [this](const std::string& id, double value) {
            if (id == "distance") setDistance(value);
        },
        {},
        {},
        [this]() { accept(); },
        [this]() { cancel(); }});
    viewport_.beginRectangleSketch(support);
    statusChanged_(QObject::tr("Sketch: set two rectangle corners"));
}

void SketchExtrusionToolController::updateDistancePreview(double distance)
{
    distance_ = distance;
    panel_.setLengthValue("distance", distance);
    statusChanged_(QObject::tr("Extrusion: %1 mm").arg(distance, 0, 'f', 3));
}

void SketchExtrusionToolController::setResult(
    const viewport::SketchRectangleResult& result)
{
    result_ = result;
    distance_ = result.distance;
    panel_.setLengthValue("distance", distance_);
    panel_.setAcceptEnabled(true);
    statusChanged_(QObject::tr("Preview ready — confirm with OK or cancel"));
}

void SketchExtrusionToolController::setDistance(double distance)
{
    distance_ = distance;
    viewport_.updateSketchExtrusionPreview(distance);
    if (result_.has_value()) {
        result_->distance = distance;
    }
}

void SketchExtrusionToolController::accept()
{
    if (!result_.has_value()) {
        return;
    }
    const viewport::PlanarFaceSelection& support = result_->support;
    const document::SketchPlane plane{
        support.originX, support.originY, support.originZ,
        support.normalX, support.normalY, support.normalZ,
        support.xDirectionX, support.xDirectionY, support.xDirectionZ};
    const document::SketchRectangle rectangle{
        result_->firstX, result_->firstY, result_->secondX, result_->secondY};
    static_cast<void>(session_.addSketchExtrusion(
        document::FeatureId{support.bodyId}, plane, rectangle, distance_));
    viewport_.cancelSketch();
    panel_.end();
    result_.reset();
    active_ = false;
    sceneChanged_();
    statusChanged_(QObject::tr("Sketch and extrusion created"));
}

void SketchExtrusionToolController::cancel()
{
    viewport_.cancelSketch();
    panel_.end();
    result_.reset();
    active_ = false;
    statusChanged_(QObject::tr("Tool cancelled"));
}

bool SketchExtrusionToolController::isActive() const noexcept { return active_; }

} // namespace voelund::desktop
