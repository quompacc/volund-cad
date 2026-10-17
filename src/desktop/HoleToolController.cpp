#include "HoleToolController.hpp"

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

HoleToolController::HoleToolController(application::CadSession& session, ToolPanelHost& panel,
                                       std::function<void()> sceneChanged,
                                       std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void HoleToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

std::optional<std::array<double, 2>>
HoleToolController::frameCoordinates(document::FeatureId support,
                                     const document::SubshapeOrigin& face,
                                     const std::array<double, 3>& point) const {
    for (const auto& candidate : session_.supportFaces(support)) {
        if (!(candidate.origin == face))
            continue;
        const auto& p = candidate.plane;
        const double dx = point[0] - p.originX, dy = point[1] - p.originY,
                     dz = point[2] - p.originZ;
        // y = normal x xDirection, the frame a sketch on the face gets.
        const double yx = p.normalY * p.xDirectionZ - p.normalZ * p.xDirectionY;
        const double yy = p.normalZ * p.xDirectionX - p.normalX * p.xDirectionZ;
        const double yz = p.normalX * p.xDirectionY - p.normalY * p.xDirectionX;
        return std::array<double, 2>{dx * p.xDirectionX + dy * p.xDirectionY + dz * p.xDirectionZ,
                                     dx * yx + dy * yy + dz * yz};
    }
    return std::nullopt;
}

void HoleToolController::begin(const viewport::PlanarFaceSelection& face) {
    // The viewport names a face by the body's latest result (its source feature).
    const document::FeatureId support{face.bodyId};
    std::optional<document::SubshapeOrigin> origin;
    try {
        origin = session_.faceReference(support, face.selectedFace).origin;
    } catch (const std::exception&) {
        statusChanged_(QObject::tr("Hier lässt sich keine Bohrung setzen. Wähle eine ebene Fläche "
                                   "eines Körpers."));
        return;
    }
    std::optional<std::array<double, 2>> at;
    if (face.pickedPoint)
        at = frameCoordinates(support, *origin, *face.pickedPoint);
    support_ = support;
    face_ = origin;
    x_ = at ? (*at)[0] : 0.0;
    y_ = at ? (*at)[1] : 0.0;
    diameter_ = 6.0;
    depth_ = 10.0;
    through_ = false;
    editing_.reset();
    stored_.reset();
    open();
}

void HoleToolController::beginEdit(document::FeatureId hole) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.holeParameters(hole);
    const auto producer = features::bodyProducerBefore(snapshot, hole, values.face.body);
    if (!producer || !session_.canAddCylinder(*producer))
        return;
    support_ = *producer;
    face_ = values.face.origin;
    x_ = values.x;
    y_ = values.y;
    diameter_ = values.diameter;
    through_ = !values.depth.has_value();
    depth_ = values.depth.value_or(10.0);
    editing_ = hole;
    stored_ = values;
    open();
}

void HoleToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Bohrung bearbeiten") : QObject::tr("Bohrung"),
        guidance(),
        {{"x", QObject::tr("X"), x_, -1'000'000.0, 1'000'000.0},
         {"y", QObject::tr("Y"), y_, -1'000'000.0, 1'000'000.0},
         {"diameter", QObject::tr("Durchmesser"), diameter_, 0.001, 1'000'000.0},
         {"depth", QObject::tr("Tiefe"), depth_, 0.001, 1'000'000.0}},
        {{"end",
          QObject::tr("Ende"),
          {QObject::tr("Tiefe"), QObject::tr("Durchgehend")},
          through_ ? 1 : 0}},
        {},
        [this](const std::string& id, double value) {
            if (id == "x")
                x_ = value;
            else if (id == "y")
                y_ = value;
            else if (id == "diameter")
                diameter_ = value;
            else if (id == "depth")
                depth_ = value;
            preview();
        },
        [this](const std::string& id, int index) {
            if (id != "end")
                return;
            through_ = index == 1;
            panel_.setLengthEnabled("depth", !through_);
            preview();
        },
        {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    panel_.setLengthEnabled("depth", !through_);
    preview();
    statusChanged_(QObject::tr("Bohrung: Lage und Maße eingeben oder auf eine Fläche klicken"));
}

void HoleToolController::place(const viewport::PlanarFaceSelection& face) {
    if (!active_ || !face.pickedPoint)
        return;
    const document::FeatureId support{face.bodyId};
    std::optional<document::SubshapeOrigin> origin;
    try {
        origin = session_.faceReference(support, face.selectedFace).origin;
    } catch (const std::exception&) {
        if (previewSession_ && previewFeature_ && support == *previewFeature_) {
            try {
                origin = previewSession_->faceReference(support, face.selectedFace).origin;
            } catch (const std::exception&) {
            }
        }
    }
    if (!origin) {
        const auto message = QObject::tr("Hier lässt sich keine Bohrung setzen. Klicke eine "
                                         "ebene Fläche des Körpers an.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    // On the previewed body the face carries the same name; its frame comes from the body
    // before the hole.
    const auto at = frameCoordinates(support_, *origin, *face.pickedPoint);
    if (!at) {
        const auto message = QObject::tr("Die Bohrung bleibt auf ihrem Körper. Klicke eine ebene "
                                         "Fläche dieses Körpers an.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    face_ = origin;
    x_ = (*at)[0];
    y_ = (*at)[1];
    panel_.setLengthValue("x", x_);
    panel_.setLengthValue("y", y_);
    preview();
}

QString HoleToolController::guidance() const {
    return through_ ? QObject::tr("Bohrung Ø %1 mm durchgehend. Ein Klick auf eine Fläche des "
                                  "Körpers setzt sie dorthin.")
                          .arg(diameter_)
                    : QObject::tr("Bohrung Ø %1 mm, %2 mm tief. Ein Klick auf eine Fläche des "
                                  "Körpers setzt sie dorthin.")
                          .arg(diameter_)
                          .arg(depth_);
}

void HoleToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

application::Action HoleToolController::action() const {
    if (editing_ && stored_)
        return application::EditHoleParametersAction{
            *editing_, document::HoleParameters{{stored_->face.body, *face_},
                                                x_,
                                                y_,
                                                diameter_,
                                                through_ ? std::nullopt : std::optional{depth_}}};
    return application::CreateHoleAction{support_,
                                         *face_,
                                         millimetres(x_),
                                         millimetres(y_),
                                         millimetres(diameter_),
                                         through_ ? std::nullopt
                                                  : std::optional{millimetres(depth_)}};
}

bool HoleToolController::unchanged() const {
    return stored_ && stored_->face.origin == *face_ && stored_->x == x_ && stored_->y == y_ &&
           stored_->diameter == diameter_ &&
           stored_->depth == (through_ ? std::nullopt : std::optional{depth_});
}

void HoleToolController::preview() {
    if (!active_ || !face_)
        return;
    if (unchanged()) {
        panel_.setAcceptEnabled(true);
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

void HoleToolController::accept() {
    if (!face_)
        return;
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
    finish();
    statusChanged_(edited ? QObject::tr("Bohrung geändert") : QObject::tr("Bohrung gesetzt"));
}

void HoleToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void HoleToolController::finish() {
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
