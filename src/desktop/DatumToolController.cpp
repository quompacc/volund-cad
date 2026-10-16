#include "DatumToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace voelund::desktop {
namespace {

double radians(double degrees) { return degrees * std::numbers::pi / 180.0; }
double degrees(double radians) { return radians * 180.0 / std::numbers::pi; }

} // namespace

DatumToolController::DatumToolController(application::CadSession& session, ToolPanelHost& panel,
                                         std::function<void()> sceneChanged,
                                         std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void DatumToolController::setPreviewHandler(PreviewHandler handler) {
    previewChanged_ = std::move(handler);
}

void DatumToolController::beginPlane(std::optional<document::DatumPlane> basePlane,
                                     std::optional<document::FeatureId> support,
                                     std::optional<document::SubshapeOrigin> face) {
    axis_ = false;
    basePlane_ = support ? std::nullopt : std::optional{basePlane.value_or(document::DatumPlane::xy)};
    support_ = support;
    face_ = face;
    faceBody_ = support ? session_.resultBody(*support) : std::nullopt;
    offset_ = 10.0;
    rotationAxis_ = document::DatumRotationAxis::none;
    angleDegrees_ = 0.0;
    editing_.reset();
    storedPlane_.reset();
    storedAxis_.reset();
    open();
}

void DatumToolController::beginAxis() {
    axis_ = true;
    baseAxis_ = document::BaseAxis::z;
    editing_.reset();
    storedPlane_.reset();
    storedAxis_.reset();
    open();
}

void DatumToolController::beginEdit(document::FeatureId datum) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto features = snapshot.features();
    const auto descriptor = std::ranges::find(features, datum, &document::FeatureDescriptor::id);
    if (descriptor == features.end())
        return;
    editing_ = datum;
    storedPlane_.reset();
    storedAxis_.reset();
    if (descriptor->kind == document::FeatureKind::datumAxis) {
        const auto& values = snapshot.datumAxisParameters(datum);
        if (!values.baseAxis) {
            statusChanged_(QObject::tr("Eine Achse auf einer Kante folgt der Kante und hat keine "
                                       "Werte zum Bearbeiten."));
            editing_.reset();
            return;
        }
        axis_ = true;
        baseAxis_ = *values.baseAxis;
        storedAxis_ = values;
    } else {
        const auto& values = snapshot.datumPlaneParameters(datum);
        axis_ = false;
        basePlane_ = values.basePlane;
        support_.reset();
        face_ = values.face ? std::optional{values.face->origin} : std::nullopt;
        faceBody_ = values.face ? std::optional{values.face->body} : std::nullopt;
        offset_ = values.offset;
        rotationAxis_ = values.rotationAxis;
        angleDegrees_ = degrees(values.angle.value());
        storedPlane_ = values;
    }
    open();
}

void DatumToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    const auto changed = [this](const std::string& id, double value) {
        if (id == "offset")
            offset_ = value;
        else if (id == "angle")
            angleDegrees_ = value;
        preview();
    };
    const auto chosen = [this](const std::string& id, int index) {
        if (id == "rotation") {
            rotationAxis_ = static_cast<document::DatumRotationAxis>(index);
            panel_.setLengthEnabled("angle", index != 0);
        } else if (id == "axis") {
            baseAxis_ = static_cast<document::BaseAxis>(index + 1);
        }
        preview();
    };
    if (axis_)
        panel_.begin(ToolPanelDefinition{
            editing_ ? QObject::tr("Bezugsachse bearbeiten") : QObject::tr("Bezugsachse"),
            QObject::tr("Wähle eine Grundachse. Eine vorher angeklickte Kante wird sofort zur "
                        "Achse."),
            {},
            {{"axis",
              QObject::tr("Achse"),
              {QObject::tr("X"), QObject::tr("Y"), QObject::tr("Z")},
              static_cast<int>(baseAxis_) - 1}},
            {},
            changed,
            chosen,
            {},
            [this]() { accept(); },
            [this]() { cancel(); },
            {}});
    else
        panel_.begin(ToolPanelDefinition{
            editing_ ? QObject::tr("Bezugsebene bearbeiten") : QObject::tr("Bezugsebene"),
            face_ ? QObject::tr("Ebene parallel zur gewählten Fläche, um den Abstand entlang "
                                "ihrer Normalen versetzt.")
                  : QObject::tr("Ebene parallel zur gewählten Grundebene, um den Abstand "
                                "versetzt."),
            {{"offset", QObject::tr("Abstand"), offset_, -1'000'000.0, 1'000'000.0},
             {"angle", QObject::tr("Winkel"), angleDegrees_, -180.0, 180.0,
              ToolValueKind::angle}},
            {{"rotation",
              QObject::tr("Drehen um"),
              {QObject::tr("Nicht drehen"), QObject::tr("X-Achse der Ebene"),
               QObject::tr("Y-Achse der Ebene")},
              static_cast<int>(rotationAxis_)}},
            {},
            changed,
            chosen,
            {},
            [this]() { accept(); },
            [this]() { cancel(); },
            {}});
    if (!axis_)
        panel_.setLengthEnabled("angle", rotationAxis_ != document::DatumRotationAxis::none);
    preview();
}

application::Action DatumToolController::action() const {
    if (axis_) {
        if (editing_)
            return application::EditDatumAxisParametersAction{
                *editing_, document::DatumAxisParameters{baseAxis_, std::nullopt, std::nullopt}};
        return application::CreateDatumAxisAction{baseAxis_, std::nullopt, std::nullopt};
    }
    const double angle =
        rotationAxis_ == document::DatumRotationAxis::none ? 0.0 : radians(angleDegrees_);
    if (editing_) {
        document::DatumPlaneParameters parameters = *storedPlane_;
        parameters.offset = offset_;
        parameters.rotationAxis = rotationAxis_;
        parameters.angle = kernel::Radians{angle};
        return application::EditDatumPlaneParametersAction{*editing_, parameters};
    }
    return application::CreateDatumPlaneAction{basePlane_,
                                               support_,
                                               face_,
                                               millimetres(offset_),
                                               rotationAxis_,
                                               {angle, application::QuantityUnit::radian}};
}

bool DatumToolController::unchanged() const {
    if (axis_)
        return storedAxis_ && storedAxis_->baseAxis == baseAxis_;
    const double angle =
        rotationAxis_ == document::DatumRotationAxis::none ? 0.0 : radians(angleDegrees_);
    return storedPlane_ && storedPlane_->offset == offset_ &&
           storedPlane_->rotationAxis == rotationAxis_ &&
           std::abs(storedPlane_->angle.value() - angle) < 1e-12;
}

void DatumToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
}

void DatumToolController::preview() {
    if (!active_)
        return;
    if (unchanged()) {
        panel_.setAcceptEnabled(true);
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
    previewSession_ = candidate.candidate;
    if (previewChanged_ && previewSession_) {
        previewChanged_(previewSession_.get());
        previewShown_ = true;
    }
}

void DatumToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (!runCommand([&] { return runAction(session_, action(), stamp_, keep_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    const bool axis = axis_;
    finish();
    statusChanged_(axis ? (edited ? QObject::tr("Bezugsachse geändert")
                                  : QObject::tr("Bezugsachse angelegt"))
                        : (edited ? QObject::tr("Bezugsebene geändert")
                                  : QObject::tr("Bezugsebene angelegt")));
}

void DatumToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void DatumToolController::finish() {
    active_ = false;
    editing_.reset();
    storedPlane_.reset();
    storedAxis_.reset();
    previewShown_ = false;
    previewSession_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
