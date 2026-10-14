#include "SketchExtrusionToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "CommandFeedback.hpp"
#include "MainWindowSupport.hpp"

#include "ToolPanelHost.hpp"

#include "ShellActions.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QObject>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace voelund::desktop {

SketchExtrusionToolController::SketchExtrusionToolController(
    application::CadSession& session, ToolPanelHost& panel, std::function<void()> sceneChanged,
    std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchExtrusionToolController::beginSelectedProfiles(bool revolve) {
    if (!session_.canExtrudeSelectedSketchProfiles())
        return;
    editing_.reset();
    stored_.reset();
    const auto selected = session_.selectedSketchProfiles();
    // T02 defaults (operation unset): a face sketch joins its body, a base-plane sketch makes
    // a new one.
    document::ExtrusionParameters values{10.0, {}};
    if (revolve) {
        // M20: a full turn about the sketch's first construction line, else about a base axis
        // in the sketch plane; without one the panel waits for a picked line.
        values.revolve = document::RevolveParameters{{}, kernel::Radians{2 * std::numbers::pi}};
        const auto sketch = session_.sketchParameters(*session_.selectedFeature());
        for (const auto& element : sketch.elements.values)
            if (element.construction && std::holds_alternative<document::SketchLine>(element.geometry)) {
                values.revolve->axis.sketchLine = element.id;
                break;
            }
        const std::pair<document::BaseAxis, double> inPlane[]{
            {document::BaseAxis::z, sketch.plane.normalZ},
            {document::BaseAxis::y, sketch.plane.normalY},
            {document::BaseAxis::x, sketch.plane.normalX}};
        if (!values.revolve->axis.sketchLine)
            for (const auto& [axis, along] : inPlane)
                if (std::abs(along) < 1e-9) {
                    values.revolve->axis.baseAxis = axis;
                    break;
                }
    }
    open(*session_.selectedFeature(), {selected.begin(), selected.end()}, values);
}

void SketchExtrusionToolController::collectAxes() {
    axes_.clear();
    axes_.push_back({QObject::tr("Linie der Skizze"), {}});
    axes_.push_back({QObject::tr("X-Achse"), {std::nullopt, document::BaseAxis::x, std::nullopt}});
    axes_.push_back({QObject::tr("Y-Achse"), {std::nullopt, document::BaseAxis::y, std::nullopt}});
    axes_.push_back({QObject::tr("Z-Achse"), {std::nullopt, document::BaseAxis::z, std::nullopt}});
    // Construction axes before the sketch (M19).
    for (const auto& feature : session_.features()) {
        if (feature.id == *profileSketch_)
            break;
        if (feature.kind == document::FeatureKind::datumAxis)
            axes_.push_back({QString::fromStdString(feature.name),
                             {std::nullopt, std::nullopt, feature.id}});
    }
}

int SketchExtrusionToolController::axisIndex() const {
    if (!revolve_ || revolve_->axis.sketchLine)
        return 0;
    for (std::size_t index = 1; index < axes_.size(); ++index)
        if (axes_[index].second == revolve_->axis)
            return static_cast<int>(index);
    return 0;
}

// M20: a click on a line of the sketch while the axis waits for one.
bool SketchExtrusionToolController::pickAxisLine(document::SketchElementId element) {
    if (!revolve_ || !axisPick_ || !profileSketch_)
        return false;
    const auto sketch = session_.sketchParameters(*profileSketch_);
    const auto found = std::ranges::find(sketch.elements.values, element,
                                         &document::SketchElement::id);
    if (found == sketch.elements.values.end() ||
        !std::holds_alternative<document::SketchLine>(found->geometry))
        return false;
    revolve_->axis = {element, std::nullopt, std::nullopt};
    axisPick_ = false;
    preview();
    return true;
}

void SketchExtrusionToolController::beginEdit(document::FeatureId extrusion) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto features = session_.features();
    const auto feature = std::ranges::find(features, extrusion, &document::FeatureDescriptor::id);
    if (feature == features.end() || feature->kind != document::FeatureKind::extrusion ||
        !feature->inputFeature)
        return;
    const auto& values = snapshot.extrusionParameters(extrusion);
    if (values.faceProfile || !values.profiles.empty())
        return; // push/pull and an unrepaired legacy selection keep their properties card
    // M20: a revolve reopens in its own mode.
    editing_ = extrusion;
    stored_ = values;
    open(*feature->inputFeature, values.regions, values);
}

void SketchExtrusionToolController::open(document::FeatureId sketchId,
                                         std::vector<document::SketchRegionId> regions,
                                         const document::ExtrusionParameters& values) {
    active_ = true;
    supportStamp_ = session_.sceneStatus().document;
    profileSketch_ = sketchId;
    profileIds_ = std::move(regions);
    // The sketch and its regions are shown and picked like a region selection (M04).
    static_cast<void>(session_.selectSketchRegions(sketchId, profileIds_));
    distance_ = values.distance;
    const auto features = session_.features();
    const auto sketch = std::ranges::find(features, *profileSketch_, &document::FeatureDescriptor::id);
    baseSketch_ = sketch != features.end() && !sketch->inputFeature;
    operation_ = document::effectiveOperation(values, baseSketch_);
    reversed_ = values.reversed;
    symmetric_ = values.symmetric;
    autoReversed_ = false;
    offset_ = values.offset;
    upToFace_ = values.upToFace.has_value();
    endFace_ = values.upToFace;
    revolve_ = values.revolve;
    if (revolve_) {
        angleDegrees_ = revolve_->angle.value() * 180.0 / std::numbers::pi;
        collectAxes();
        const auto& axis = revolve_->axis;
        axisPick_ = !axis.sketchLine && !axis.baseAxis && !axis.datumAxis;
    }
    // A base-plane sketch joins or cuts a named body; the field shows when there is a choice.
    targets_.clear();
    std::vector<QString> targetNames;
    const auto outline = session_.modelOutline();
    for (const auto& body : outline.bodies) {
        if (!baseSketch_ || std::ranges::find(session_.validBodies(), body.id,
                                              &features::EvaluatedBody::bodyId) ==
                                session_.validBodies().end())
            continue;
        targets_.push_back(body.id);
        targetNames.push_back(bodyLabel(outline, body.id)); // M01: named as in the tree
    }
    target_ = targets_.empty() ? std::nullopt : std::optional{targets_.front()};
    int targetIndex = 0;
    if (values.target)
        for (std::size_t index = 0; index < targets_.size(); ++index)
            if (targets_[index] == *values.target) {
                target_ = targets_[index];
                targetIndex = static_cast<int>(index);
            }
    std::vector<ToolChoiceParameter> choices{
        {"end", QObject::tr("Ende"), {QObject::tr("Distanz"), QObject::tr("Bis Fläche")},
         upToFace_ ? 1 : 0},
        {"direction",
         QObject::tr("Richtung"),
         // M02 (user decision 2026-10-11): symmetric = the distance as total length.
         {QObject::tr("Entlang Normale"), QObject::tr("Entgegen Normale"),
          QObject::tr("Symmetrisch")},
         symmetric_ ? 2 : reversed_ ? 1 : 0},
        {"operation",
         QObject::tr("Ergebnis"),
         {QObject::tr("Neuer Körper"), QObject::tr("Vereinigen"), QObject::tr("Abziehen"),
          QObject::tr("Schneiden")},
         static_cast<int>(operation_) - 1}};
    if (targets_.size() > 1)
        choices.push_back({"target", QObject::tr("Zielkörper"), targetNames, targetIndex});
    std::vector<ToolLengthParameter> lengths{
        {"distance", QObject::tr("Distanz"), distance_, 0.001, 1'000'000.0},
        {"offset", QObject::tr("Versatz"), offset_, -1'000'000.0, 1'000'000.0}};
    if (revolve_) {
        // M20: angle and axis instead of length, offset and end face.
        choices.erase(choices.begin());
        std::vector<QString> axisNames;
        for (const auto& axis : axes_)
            axisNames.push_back(axis.first);
        choices.insert(choices.begin(), {"axis", QObject::tr("Achse"), axisNames, axisIndex()});
        lengths = {{"angle", QObject::tr("Winkel"), angleDegrees_, 0.001, 360.0,
                    ToolValueKind::angle}};
    }
    panel_.begin(ToolPanelDefinition{
        revolve_ ? (editing_ ? QObject::tr("Drehung bearbeiten") : QObject::tr("Drehen"))
                 : (editing_ ? QObject::tr("Extrusion bearbeiten") : QObject::tr("Extrudieren")),
        QObject::tr("%1 gewählte(n) Skizzenbereich(e) extrudieren.").arg(profileIds_.size()),
        std::move(lengths),
        std::move(choices),
        {},
        [this](const std::string& id, double value) {
            if (id == "distance")
                setDistance(value);
            else if (id == "offset")
                offset_ = value;
            else if (id == "angle" && revolve_) {
                angleDegrees_ = value;
                revolve_->angle = kernel::Radians{value * std::numbers::pi / 180.0};
            }
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
    panel_.setLengthEnabled("distance", !upToFace_);
    syncFields();
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
    if (editing_ && stored_) {
        // M04: the stored extrusion with the panel's values; a changed length leaves its
        // parameter binding.
        auto values = *stored_;
        if (distance_ != values.distance)
            values.distanceParameter.reset();
        if (offset_ != values.offset)
            values.offsetParameter.reset();
        values.distance = distance_;
        values.offset = offset_;
        values.regions = profileIds_;
        values.operation = operation_;
        values.reversed = reversed_;
        values.symmetric = symmetric_;
        values.target = named ? target_ : std::nullopt;
        values.upToFace = upToFace_ ? endFace_ : std::nullopt;
        values.revolve = revolve_;
        return application::EditExtrusionParametersAction{*editing_, values};
    }
    return application::CreateProfileExtrusionAction{
        *profileSketch_, profileIds_, millimetres(distance_),
        {operation_, reversed_, named ? target_ : std::nullopt, offset_,
         upToFace_ ? endFace_ : std::nullopt, symmetric_, revolve_}};
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
    if ((upToFace_ && !endFace_) || axisPick_) {
        panel_.setAcceptEnabled(false);
        panel_.setGuidance(guidance());
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, extrusion(), supportStamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setAcceptEnabled(ok);
    problemElements_.clear();
    for (const auto& issue : result.evaluation().features)
        if (issue.code.starts_with("region-"))
            problemElements_.insert(problemElements_.end(), issue.elements.begin(),
                                    issue.elements.end());
    if (!ok) {
        panel_.setGuidance(commandFeedbackMessage(result));
        showModel();
        return;
    }
    panel_.setGuidance(guidance());
    previewChanged_(&candidate.bodies);
    previewShown_ = true;
}

void SketchExtrusionToolController::choose(const std::string& choice, int index) {
    if (choice == "end") {
        upToFace_ = index == 1;
        panel_.setLengthEnabled("distance", !upToFace_);
        panel_.setAcceptEnabled(!upToFace_ || endFace_.has_value());
        panel_.setGuidance(guidance());
    } else if (choice == "direction") {
        reversed_ = index == 1;
        symmetric_ = index == 2;
        autoReversed_ = false; // the user's own direction stays
    } else if (choice == "axis" && revolve_ && index >= 0 &&
               static_cast<std::size_t>(index) < axes_.size()) {
        // M20: the first entry waits for a line clicked in the sketch.
        axisPick_ = index == 0;
        if (index != 0)
            revolve_->axis = axes_[static_cast<std::size_t>(index)].second;
    } else if (choice == "operation") {
        operation_ = static_cast<document::ExtrusionOperation>(index + 1);
        // A cut goes into the material; the panel shows the changed direction and turns it
        // back when the cut is left again (M04), unless the user chose it. A revolve's
        // direction is the turning sense, which a cut does not change.
        if (!revolve_ && operation_ == document::ExtrusionOperation::cut && !reversed_ &&
            !symmetric_) {
            panel_.chooseOption("direction", 1);
            autoReversed_ = true;
        } else if (operation_ != document::ExtrusionOperation::cut && autoReversed_) {
            panel_.chooseOption("direction", 0);
            autoReversed_ = false;
        }
    } else if (choice == "target" && index >= 0 &&
               static_cast<std::size_t>(index) < targets_.size()) {
        target_ = targets_[static_cast<std::size_t>(index)];
    }
    syncFields();
}

void SketchExtrusionToolController::syncFields() {
    // Symmetric (M02): the distance is the total length; no start offset and no end face.
    panel_.setLengthEnabled("offset", !symmetric_);
    if (symmetric_ && offset_ != 0.0) {
        offset_ = 0.0;
        panel_.setLengthValue("offset", 0.0);
    }
    panel_.setChoiceEnabled("end", !symmetric_);
    // A new body has no target.
    panel_.setChoiceVisible("target", operation_ != document::ExtrusionOperation::newBody);
}

QString SketchExtrusionToolController::guidance() const {
    if (upToFace_ && !endFace_)
        return QObject::tr("Wähle im Modell die Fläche, an der die Extrusion endet.");
    if (axisPick_)
        return QObject::tr("Klicke eine Linie der Skizze als Drehachse an.");
    QString text = revolve_ ? QObject::tr("%1 gewählte(n) Skizzenbereich(e) um %2° drehen.")
                                  .arg(profileIds_.size())
                                  .arg(angleDegrees_)
                            : QObject::tr("%1 gewählte(n) Skizzenbereich(e) extrudieren.")
                                  .arg(profileIds_.size());
    if (operation_ == document::ExtrusionOperation::newBody)
        return text + QObject::tr(" Ergebnis: neuer Körper.");
    // M04 (A-BODY-03 step 4): the body the operation works on, by its name.
    const auto outline = session_.modelOutline();
    std::optional<document::BodyId> body = baseSketch_ ? target_ : std::nullopt;
    if (!baseSketch_ && profileSketch_)
        for (const auto& item : outline.bodies)
            if (std::ranges::find(item.features, *profileSketch_) != item.features.end())
                body = item.id;
    if (body)
        text += QObject::tr(" Ziel: %1.").arg(bodyLabel(outline, *body));
    return text;
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
    const bool edited = editing_.has_value();
    const bool turned = revolve_.has_value();
    revolve_.reset();
    axisPick_ = false;
    problemElements_.clear();
    if (editing_)
        static_cast<void>(session_.selectFeature(*editing_));
    editing_.reset();
    stored_.reset();
    sceneChanged_();
    statusChanged_(turned ? (edited ? QObject::tr("Drehung geändert")
                                    : QObject::tr("Gewählte Bereiche gedreht"))
                          : (edited ? QObject::tr("Extrusion geändert")
                                    : QObject::tr("Gewählte Bereiche extrudiert")));
}

void SketchExtrusionToolController::cancel() {
    problemElements_.clear();
    revolve_.reset();
    axisPick_ = false;
    if (editing_)
        static_cast<void>(session_.selectFeature(*editing_));
    editing_.reset();
    stored_.reset();
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

std::optional<document::FeatureId> SketchExtrusionToolController::regionSketch() const noexcept {
    // "Bis Fläche" picks faces; a click there must not change the regions.
    return active_ && !upToFace_ ? profileSketch_ : std::nullopt;
}

const std::vector<std::uint64_t>& SketchExtrusionToolController::problemElements() const noexcept {
    return problemElements_;
}

// The preview body in front of the sketch does not hide its regions.
bool SketchExtrusionToolController::toggleRegionAt(const viewport::ViewRay& ray) {
    const auto sketch = regionSketch();
    const auto crossing = sketch ? sketchPlaneCrossing(session_, *sketch, ray) : std::nullopt;
    if (!crossing || !session_.selectSketchProfileAt(*sketch, crossing->point, true))
        return false;
    regionsToggled();
    return true;
}

bool SketchExtrusionToolController::toggleRegionByElement(document::SketchElementId element) {
    if (pickAxisLine(element))
        return true;
    if (!regionSketch() || !session_.selectSketchProfileByElement(element, true))
        return false;
    regionsToggled();
    return true;
}

void SketchExtrusionToolController::regionsToggled() {
    if (!active_)
        return;
    const auto selected = session_.selectedSketchProfiles();
    profileIds_.assign(selected.begin(), selected.end());
    if (!profileIds_.empty()) {
        preview();
        return;
    }
    panel_.setAcceptEnabled(false);
    problemElements_.clear();
    panel_.setGuidance(QObject::tr("Kein Bereich gewählt. Klicke einen Skizzenbereich an, um "
                                   "ihn wieder aufzunehmen."));
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
}

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
