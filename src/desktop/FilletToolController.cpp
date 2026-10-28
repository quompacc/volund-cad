// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FilletToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>

#include <QObject>
#include <QTimer>

#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <cmath>

#include <algorithm>
#include <utility>

namespace voelund::desktop {

FilletToolController::FilletToolController(application::CadSession& session,
                                           viewport::CadViewport& viewport, ToolPanelHost& panel,
                                           std::function<void()> sceneChanged,
                                           std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void FilletToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

void FilletToolController::begin(document::FeatureId support, EdgeTreatment treatment,
                                 std::vector<document::FilletEdge> edges) {
    if (!session_.canAddCylinder(support))
        return;
    treatment_ = treatment;
    editing_.reset();
    stored_.reset();
    open(support, std::move(edges), 1.0);
}

void FilletToolController::beginEdit(document::FeatureId feature) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto features = snapshot.features();
    const auto descriptor = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (descriptor == features.end())
        return;
    std::optional<Stored> found;
    if (descriptor->kind == document::FeatureKind::fillet) {
        const auto& fillet = snapshot.filletParameters(feature);
        found = Stored{fillet.body, fillet.edges, fillet.radius};
        treatment_ = EdgeTreatment::fillet;
    } else if (descriptor->kind == document::FeatureKind::chamfer) {
        const auto& chamfer = snapshot.chamferParameters(feature);
        found = Stored{chamfer.body, chamfer.edges, chamfer.distance, chamfer.shape};
        treatment_ = EdgeTreatment::chamfer;
    } else {
        return;
    }
    const Stored& values = *found;
    // The edges are named on the body as it was before the feature.
    const auto producer = features::bodyProducerBefore(snapshot, feature, values.body);
    if (!producer || !session_.canAddCylinder(*producer))
        return;
    editing_ = feature;
    stored_ = values;
    open(*producer, values.edges, values.value);
}

void FilletToolController::open(document::FeatureId support,
                                std::vector<document::FilletEdge> edges, double value) {
    const bool chamfer = treatment_ == EdgeTreatment::chamfer;
    active_ = true;
    support_ = support;
    edges_ = std::move(edges);
    features::numberFilletEdges(edges_);
    value_ = value;
    shape_ = stored_ ? stored_->shape : document::ChamferShape{};
    stamp_ = session_.sceneStatus().document;
    const QString title =
        chamfer ? (editing_ ? QObject::tr("Fase bearbeiten") : QObject::tr("Fase"))
                : (editing_ ? QObject::tr("Verrundung bearbeiten") : QObject::tr("Verrunden"));
    const std::string field = chamfer ? "distance" : "radius";
    panel_.begin(withFieldBindings(definition(title, field), editing_,
                                   [this] { stamp_ = session_.sceneStatus().document; }));
    panel_.focusLength(field); // W05: the main value takes the keyboard
    showChamferFields();
    viewport_.setEdgePicking(true);
    viewport_.setPushPullHandler([this](double to, bool finished) { dragged(to, finished); });
    gripEdge_.reset();
    preview();
    statusChanged_(chamfer ? QObject::tr("Fase: Kanten anklicken, Abstand eingeben")
                           : QObject::tr("Verrunden: Kanten anklicken, Radius eingeben"));
}

// The panel: the radius or distance; N10: a chamfer's kind, its second distance or angle and
// "Richtung umkehren" (the first distance on the other face of each edge).
ToolPanelDefinition FilletToolController::definition(const QString& title,
                                                     const std::string& field) {
    const bool chamfer = treatment_ == EdgeTreatment::chamfer;
    ToolPanelDefinition result{
        title,
        guidance(),
        {{field, chamfer ? QObject::tr("Abstand") : QObject::tr("Radius"), value_, 0.001,
          1'000'000.0}},
        {},
        {},
        [this, field](const std::string& id, double changed) { changedValue(field, id, changed); },
        [this](const std::string& id, int index) {
            if (id == "chamferKind")
                chooseChamferKind(index);
        },
        [this](const std::string& id) {
            if (id == "flip") {
                shape_.flipped = !shape_.flipped;
                preview();
            }
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        {}};
    if (!chamfer)
        return result;
    const double second = shape_.second > 0.0 ? shape_.second : value_;
    result.lengthParameters.push_back(
        {"second", QObject::tr("Zweiter Abstand"), second, 0.001, 1'000'000.0});
    result.lengthParameters.push_back({"angle", QObject::tr("Winkel"),
                                       shape_.second > 0.0 ? shape_.second : 45.0, 0.1, 89.9,
                                       ToolValueKind::angle});
    result.choiceParameters.push_back(
        {"chamferKind",
         QObject::tr("Art"),
         {QObject::tr("Gleiche Abstände"), QObject::tr("Zwei Abstände"),
          QObject::tr("Abstand und Winkel")},
         static_cast<int>(shape_.mode)});
    result.actions.push_back({"flip", QObject::tr("Richtung umkehren"), QObject::tr("Fase")});
    return result;
}

void FilletToolController::changedValue(const std::string& field, const std::string& id,
                                        double value) {
    if (id == field)
        value_ = value;
    else if (id == "second" || id == "angle")
        shape_.second = value;
    else
        return;
    preview();
}

// N10: a new kind starts from the shown values (the same second distance, 45 degrees).
void FilletToolController::chooseChamferKind(int index) {
    shape_.mode = static_cast<document::ChamferMode>(index);
    shape_.second = shape_.mode == document::ChamferMode::distanceAngle  ? 45.0
                    : shape_.mode == document::ChamferMode::twoDistances ? value_
                                                                         : 0.0;
    panel_.setLengthValue(shape_.mode == document::ChamferMode::distanceAngle ? "angle" : "second",
                          shape_.second);
    showChamferFields();
    preview();
}

void FilletToolController::showChamferFields() {
    if (treatment_ != EdgeTreatment::chamfer)
        return;
    panel_.setLengthVisible("second", shape_.mode == document::ChamferMode::twoDistances);
    panel_.setLengthVisible("angle", shape_.mode == document::ChamferMode::distanceAngle);
    panel_.setActionEnabled("flip", shape_.mode != document::ChamferMode::equal);
}

QString FilletToolController::guidance() const {
    const bool chamfer = treatment_ == EdgeTreatment::chamfer;
    if (edges_.empty())
        return chamfer ? QObject::tr("Klicke die Kanten an, die angefast werden sollen.")
                       : QObject::tr("Klicke die Kanten an, die verrundet werden sollen.");
    return (chamfer ? QObject::tr("%1 Kante(n) mit Abstand %2 mm. Ein Klick auf eine weitere "
                                  "Kante nimmt sie dazu, auf eine gewählte nimmt sie heraus.")
                    : QObject::tr("%1 Kante(n) mit Radius %2 mm. Ein Klick auf eine weitere "
                                  "Kante nimmt sie dazu, auf eine gewählte nimmt sie heraus."))
        .arg(edges_.size())
        .arg(value_);
}

void FilletToolController::picked(std::uint64_t body, const TopoDS_Shape& edge) {
    if (!active_)
        return;
    if (body == 0) // a chosen edge drawn over the preview
        return removeChosen(edge);
    followBody(body);
    const auto named = namedEdge(edge);
    if (!named) {
        const auto message = QObject::tr("Diese Kante lässt sich nicht wählen: Sie gehört nicht "
                                         "zum gewählten Körper oder grenzt an eine neue "
                                         "Rundungs- oder Fasenfläche.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    const auto known = std::ranges::find_if(
        edges_, [&](const auto& other) { return features::sameFilletEdge(other, *named); });
    if (known != edges_.end())
        edges_.erase(known);
    else
        edges_.push_back(*named);
    features::numberFilletEdges(edges_); // a new edge gets the next free id
    preview();
}

void FilletToolController::pickedChain(std::uint64_t body, const TopoDS_Shape& edge) {
    if (!active_ || body == 0)
        return;
    followBody(body);
    const auto original = originalEdge(edge);
    const auto bodies = session_.validBodies();
    const auto owner = std::ranges::find(bodies, support_, &features::EvaluatedBody::sourceFeature);
    if (owner == bodies.end() || original.IsNull())
        return;
    for (const auto& link : kernel::tangentChain(owner->shape, original))
        if (const auto named = session_.filletEdge(support_, link))
            addEdge(*named);
    features::numberFilletEdges(edges_);
    preview();
}

// W07: the named edges among a box's edges join the chosen ones (none is dropped).
void FilletToolController::pickedEdges(std::uint64_t body, std::span<const TopoDS_Shape> edges) {
    if (!active_ || body == 0)
        return;
    followBody(body);
    // Only edges found on the body before the rounding (not those of a new round in the preview).
    for (const auto& edge : edges)
        if (const auto original = originalEdge(edge); !original.IsNull())
            if (const auto named = session_.filletEdge(support_, original))
                addEdge(*named);
    features::numberFilletEdges(edges_);
    preview();
}

void FilletToolController::addEdge(const document::FilletEdge& named) {
    if (std::ranges::none_of(
            edges_, [&](const auto& chosen) { return features::sameFilletEdge(chosen, named); }))
        edges_.push_back(named);
}

// A clicked edge's name on the support, also for an edge of the preview body.
std::optional<document::FilletEdge>
FilletToolController::namedEdge(const TopoDS_Shape& edge) const {
    auto named = session_.filletEdge(support_, edge);
    if (!named && previewSession_ && previewFeature_)
        named = previewSession_->filletEdge(*previewFeature_, edge);
    return named;
}

// The body's edge before the rounding where a clicked edge lies (also one of the preview, which
// rounds the chosen edges already); null when the edge has no name.
TopoDS_Shape FilletToolController::originalEdge(const TopoDS_Shape& edge) const {
    const auto clicked = namedEdge(edge);
    if (!clicked)
        return {};
    const auto shapes = session_.filletEdgeShapes(support_, std::span(&*clicked, 1));
    return shapes.empty() ? TopoDS_Shape() : shapes.front();
}

// Opened with nothing selected: the first edge clicked chooses the body (as in Fusion).
void FilletToolController::followBody(std::uint64_t body) {
    if (edges_.empty() && !editing_ && body != support_.value())
        support_ = document::FeatureId{body};
}

// Removes the named edge a chosen edge drawn over the preview belongs to.
void FilletToolController::removeChosen(const TopoDS_Shape& edge) {
    const auto chosen = std::ranges::find_if(edges_, [&](const auto& named) {
        const auto shapes = session_.filletEdgeShapes(support_, std::span(&named, 1));
        return std::ranges::any_of(shapes, [&](const auto& shape) { return shape.IsSame(edge); });
    });
    if (chosen != edges_.end())
        edges_.erase(chosen);
    preview();
}

void FilletToolController::placeGrip() {
    if (edges_.empty()) {
        if (gripEdge_)
            viewport_.endPushPull();
        gripEdge_.reset();
        return;
    }
    if (gripEdge_ && features::sameFilletEdge(*gripEdge_, edges_.front())) {
        viewport_.setPushPullDistance(value_);
        return;
    }
    // Into the corner: against the sum of the two faces' outward normals, at the edge's middle
    // (user report 2026-10-25: curved faces too, a round edge has its arrow).
    const auto shapes = session_.filletEdgeShapes(support_, std::span(&edges_.front(), 1));
    if (gripEdge_)
        viewport_.endPushPull();
    gripEdge_.reset();
    const auto body = std::ranges::find(session_.validBodies(), support_,
                                        &features::EvaluatedBody::sourceFeature);
    const auto direction = shapes.empty() || body == session_.validBodies().end()
                               ? std::nullopt
                               : kernel::intoEdgeCorner(body->shape, shapes.front());
    if (!direction)
        return; // the value is typed only
    viewport_.beginEdgeGrip(kernel::collect(shapes),
                            {direction->X(), direction->Y(), direction->Z()});
    viewport_.setPushPullDistance(value_);
    gripEdge_ = edges_.front();
}

void FilletToolController::dragged(double value, bool finished) {
    if (!active_ || !gripEdge_)
        return;
    value_ = std::max(value, 0.01);
    panel_.setLengthValue(treatment_ == EdgeTreatment::chamfer ? "distance" : "radius", value_);
    if (finished) {
        preview();
        return;
    }
    // The body follows the grip while dragging; previews are bundled to one per 40 ms.
    if (previewPending_)
        return;
    previewPending_ = true;
    QTimer::singleShot(40, &panel_, [this] {
        previewPending_ = false;
        preview();
    });
}

void FilletToolController::highlight(const std::vector<std::uint64_t>& failing) {
    std::vector<TopoDS_Shape> chosen;
    std::vector<TopoDS_Shape> faulty;
    for (std::size_t index = 0; index < edges_.size(); ++index) {
        const auto shapes = session_.filletEdgeShapes(support_, std::span(&edges_[index], 1));
        auto& target = std::ranges::find(failing, index + 1) != failing.end() ? faulty : chosen;
        target.insert(target.end(), shapes.begin(), shapes.end());
    }
    viewport_.setHighlightedEdges(chosen, faulty);
}

void FilletToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

void FilletToolController::preview() {
    if (!active_)
        return;
    placeGrip();
    if (edges_.empty()) {
        panel_.setAcceptEnabled(false);
        panel_.setGuidance(guidance());
        highlight({});
        showModel();
        return;
    }
    if (unchanged()) {
        // An edit that changes nothing shows the model as it is; OK just closes.
        panel_.setAcceptEnabled(true);
        panel_.setGuidance(guidance());
        highlight({});
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, action(), stamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setErrorCommitHandler({});
    panel_.setAcceptEnabled(ok);
    const std::string prefix = treatment_ == EdgeTreatment::chamfer ? "chamfer-" : "fillet-";
    std::vector<std::uint64_t> failing;
    for (const auto& issue : result.evaluation().features)
        if (issue.code.starts_with(prefix))
            failing.insert(failing.end(), issue.elements.begin(), issue.elements.end());
    highlight(failing);
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

application::Action FilletToolController::action() const {
    if (treatment_ == EdgeTreatment::chamfer) {
        if (editing_ && stored_)
            return application::EditChamferParametersAction{
                *editing_, document::ChamferParameters{stored_->body, edges_, value_, shape_}};
        return application::CreateChamferAction{support_, edges_, millimetres(value_), shape_};
    }
    if (editing_ && stored_)
        return application::EditFilletParametersAction{
            *editing_, document::FilletParameters{stored_->body, edges_, value_}};
    return application::CreateFilletAction{support_, edges_, millimetres(value_)};
}

bool FilletToolController::unchanged() const {
    return stored_ && stored_->edges == edges_ && stored_->value == value_ &&
           stored_->shape == shape_;
}

void FilletToolController::accept() {
    if (edges_.empty())
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
    const bool chamfer = treatment_ == EdgeTreatment::chamfer;
    finish();
    statusChanged_(
        chamfer ? (edited ? QObject::tr("Fase geändert") : QObject::tr("Kanten angefast"))
                : (edited ? QObject::tr("Verrundung geändert") : QObject::tr("Kanten verrundet")));
}

void FilletToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void FilletToolController::finish() {
    active_ = false;
    edges_.clear();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
    if (gripEdge_)
        viewport_.endPushPull();
    gripEdge_.reset();
    viewport_.setHighlightedEdges({}, {});
    viewport_.setEdgePicking(false);
    panel_.end();
    // The scene shows the model again instead of the preview.
    sceneChanged_();
}

} // namespace voelund::desktop
