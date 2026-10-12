#include "FilletToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>

#include <QObject>

#include <algorithm>
#include <utility>

namespace voelund::desktop {

FilletToolController::FilletToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel),
      sceneChanged_(std::move(sceneChanged)), statusChanged_(std::move(statusChanged)) {
    viewport_.setEdgeSelectionHandler(
        [this](std::uint64_t body, const TopoDS_Shape& edge) { picked(body, edge); });
}

void FilletToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

void FilletToolController::begin(document::FeatureId support, EdgeTreatment treatment) {
    if (!session_.canAddCylinder(support))
        return;
    treatment_ = treatment;
    editing_.reset();
    stored_.reset();
    open(support, {}, 1.0);
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
        found = Stored{chamfer.body, chamfer.edges, chamfer.distance};
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
    value_ = value;
    stamp_ = session_.sceneStatus().document;
    const QString title = chamfer ? (editing_ ? QObject::tr("Fase bearbeiten") : QObject::tr("Fase"))
                                  : (editing_ ? QObject::tr("Verrundung bearbeiten")
                                              : QObject::tr("Verrunden"));
    const std::string field = chamfer ? "distance" : "radius";
    panel_.begin(ToolPanelDefinition{
        title,
        guidance(),
        {{field, chamfer ? QObject::tr("Abstand") : QObject::tr("Radius"), value_, 0.001,
          1'000'000.0}},
        {},
        {},
        [this, field](const std::string& id, double changed) {
            if (id == field) {
                value_ = changed;
                preview();
            }
        },
        {},
        {},
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    viewport_.setEdgePicking(true);
    preview();
    statusChanged_(chamfer ? QObject::tr("Fase: Kanten anklicken, Abstand eingeben")
                           : QObject::tr("Verrunden: Kanten anklicken, Radius eingeben"));
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
    if (body == 0) {
        // A chosen edge drawn over the preview: remove the named edge it belongs to.
        const auto chosen = std::ranges::find_if(edges_, [&](const auto& named) {
            const auto shapes = session_.filletEdgeShapes(support_, std::span(&named, 1));
            return std::ranges::any_of(shapes, [&](const auto& shape) { return shape.IsSame(edge); });
        });
        if (chosen != edges_.end())
            edges_.erase(chosen);
        preview();
        return;
    }
    const auto named = session_.filletEdge(support_, edge);
    if (!named) {
        const auto message = QObject::tr("Diese Kante gehört nicht zum gewählten Körper oder "
                                         "liegt nicht zwischen zwei seiner Flächen.");
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
    preview();
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
}

void FilletToolController::preview() {
    if (!active_)
        return;
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
    panel_.setAcceptEnabled(ok);
    const std::string prefix = treatment_ == EdgeTreatment::chamfer ? "chamfer-" : "fillet-";
    std::vector<std::uint64_t> failing;
    for (const auto& issue : result.evaluation().features)
        if (issue.code.starts_with(prefix))
            failing.insert(failing.end(), issue.elements.begin(), issue.elements.end());
    highlight(failing);
    if (!ok) {
        panel_.setGuidance(commandFeedbackMessage(result));
        showModel();
        return;
    }
    panel_.setGuidance(guidance());
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

application::Action FilletToolController::action() const {
    if (treatment_ == EdgeTreatment::chamfer) {
        if (editing_ && stored_)
            return application::EditChamferParametersAction{
                *editing_, document::ChamferParameters{stored_->body, edges_, value_}};
        return application::CreateChamferAction{support_, edges_, millimetres(value_)};
    }
    if (editing_ && stored_)
        return application::EditFilletParametersAction{
            *editing_, document::FilletParameters{stored_->body, edges_, value_}};
    return application::CreateFilletAction{support_, edges_, millimetres(value_)};
}

bool FilletToolController::unchanged() const {
    return stored_ && stored_->edges == edges_ && stored_->value == value_;
}

void FilletToolController::accept() {
    if (edges_.empty())
        return;
    if (unchanged()) {
        finish();
        return;
    }
    if (!runCommand([&] { return runAction(session_, action(), stamp_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    const bool chamfer = treatment_ == EdgeTreatment::chamfer;
    finish();
    statusChanged_(chamfer ? (edited ? QObject::tr("Fase geändert") : QObject::tr("Kanten angefast"))
                           : (edited ? QObject::tr("Verrundung geändert")
                                     : QObject::tr("Kanten verrundet")));
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
    viewport_.setHighlightedEdges({}, {});
    viewport_.setEdgePicking(false);
    panel_.end();
    // The scene shows the model again instead of the preview.
    sceneChanged_();
}

} // namespace voelund::desktop
