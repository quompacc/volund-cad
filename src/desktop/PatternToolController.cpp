// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "PatternToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>

#include <QObject>

#include <algorithm>
#include <cmath>
#include <exception>
#include <numbers>
#include <span>
#include <utility>

namespace voelund::desktop {
namespace {
constexpr int baseAxisCount = 3;
constexpr double degree = std::numbers::pi / 180.0;
} // namespace

PatternToolController::PatternToolController(application::CadSession& session,
                                             viewport::CadViewport& viewport, ToolPanelHost& panel,
                                             std::function<void()> sceneChanged,
                                             std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void PatternToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

void PatternToolController::begin(document::FeatureId support, document::PatternKind kind,
                                  std::optional<document::FeatureId> edgeSupport,
                                  std::optional<document::FilletEdge> edge) {
    support_ = support;
    kind_ = kind;
    editing_.reset();
    stored_.reset();
    reversed_ = false;
    mode_ = document::PatternMode::newBodies;
    count_ = kind == document::PatternKind::linear ? 3 : 4;
    spacing_ = 20.0;
    angleDegrees_ = 360.0;
    datumAxes_.clear();
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    for (const auto& feature : snapshot.features())
        if (feature.kind == document::FeatureKind::datumAxis)
            datumAxes_.push_back(feature);
    edge_.reset();
    if (edgeSupport && edge)
        if (const auto body = session_.resultBody(*edgeSupport))
            edge_ = PickedEdge{*edgeSupport, *body, *edge};
    // Linear along X, circular about Z, unless an edge was picked before.
    axisIndex_ = edge_ ? edgeChoice() : kind == document::PatternKind::linear ? 0 : 2;
    open();
}

void PatternToolController::beginEdit(document::FeatureId pattern) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.patternParameters(pattern);
    const auto producer = features::bodyProducerBefore(snapshot, pattern, values.body);
    if (!producer)
        return;
    support_ = *producer;
    editing_ = pattern;
    stored_ = values;
    kind_ = values.kind;
    reversed_ = values.reversed;
    mode_ = values.mode;
    count_ = values.count;
    spacing_ = values.spacing;
    angleDegrees_ = values.angle.value() / degree;
    datumAxes_.clear();
    for (const auto& feature : snapshot.features()) {
        if (feature.id == pattern)
            break;
        if (feature.kind == document::FeatureKind::datumAxis)
            datumAxes_.push_back(feature);
    }
    edge_.reset();
    if (values.axis.baseAxis) {
        axisIndex_ = static_cast<int>(*values.axis.baseAxis) - 1;
    } else if (values.datumAxis) {
        const auto found =
            std::ranges::find(datumAxes_, *values.datumAxis, &document::FeatureDescriptor::id);
        axisIndex_ = baseAxisCount + static_cast<int>(found - datumAxes_.begin());
    } else {
        const auto edgeProducer =
            features::bodyProducerBefore(snapshot, pattern, *values.axis.body);
        edge_ = PickedEdge{edgeProducer.value_or(*producer), *values.axis.body, *values.axis.edge};
        axisIndex_ = edgeChoice();
    }
    open();
}

int PatternToolController::edgeChoice() const noexcept {
    return baseAxisCount + static_cast<int>(datumAxes_.size());
}

bool PatternToolController::hasAxis() const noexcept {
    return axisIndex_ < edgeChoice() || edge_.has_value();
}

void PatternToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    std::vector<QString> axes{QObject::tr("X-Achse"), QObject::tr("Y-Achse"),
                              QObject::tr("Z-Achse")};
    for (const auto& feature : datumAxes_)
        axes.push_back(QString::fromStdString(feature.name));
    axes.push_back(QObject::tr("Kante (anklicken)"));
    panel_.begin(withFieldBindings(
        ToolPanelDefinition{
            editing_ ? QObject::tr("Muster bearbeiten") : QObject::tr("Muster"),
            guidance(),
            {{"count", QObject::tr("Anzahl"), count_, 2.0, 100.0, ToolValueKind::count},
             {"spacing", QObject::tr("Abstand"), spacing_, 0.001, 1'000'000.0},
             {"angle", QObject::tr("Winkel"), angleDegrees_, 0.001, 360.0, ToolValueKind::angle}},
            {{"kind",
              QObject::tr("Art"),
              {QObject::tr("Linear"), QObject::tr("Kreisförmig")},
              kind_ == document::PatternKind::circular ? 1 : 0},
             {"axis", QObject::tr("Richtung/Achse"), axes, axisIndex_},
             {"direction",
              QObject::tr("Richtung"),
              {QObject::tr("Normal"), QObject::tr("Umgekehrt")},
              reversed_ ? 1 : 0},
             {"mode",
              QObject::tr("Ergebnis"),
              {QObject::tr("Neue Körper"), QObject::tr("Vereinigen")},
              mode_ == document::PatternMode::join ? 1 : 0}},
            {},
            [this](const std::string& id, double value) {
                if (id == "count")
                    count_ = std::round(value);
                if (id == "spacing")
                    spacing_ = value;
                if (id == "angle")
                    angleDegrees_ = value;
                preview();
            },
            [this](const std::string& id, int index) {
                if (id == "kind")
                    kind_ = index == 1 ? document::PatternKind::circular
                                       : document::PatternKind::linear;
                if (id == "axis")
                    axisIndex_ = index;
                if (id == "direction")
                    reversed_ = index == 1;
                if (id == "mode")
                    mode_ =
                        index == 1 ? document::PatternMode::join : document::PatternMode::newBodies;
                syncFields();
                preview();
                highlightEdge();
            },
            [](const std::string&) {},
            [this]() { accept(); },
            [this]() { cancel(); },
            {}},
        editing_, [this] { stamp_ = session_.sceneStatus().document; }));
    syncFields();
    viewport_.setEdgePicking(true);
    preview();
    highlightEdge();
    statusChanged_(QObject::tr("Muster: Achse wählen oder eine Kante anklicken"));
}

// The clicked edge, while it is the axis.
void PatternToolController::highlightEdge() {
    if (!edge_ || axisIndex_ != edgeChoice()) {
        viewport_.setHighlightedEdges({});
        return;
    }
    viewport_.setHighlightedEdges(
        session_.filletEdgeShapes(edge_->support, std::span{&edge_->edge, 1}));
}

// A linear pattern has a spacing, a circular one an angle.
void PatternToolController::syncFields() {
    panel_.setLengthVisible("spacing", kind_ == document::PatternKind::linear);
    panel_.setLengthVisible("angle", kind_ == document::PatternKind::circular);
}

void PatternToolController::picked(std::uint64_t body, const TopoDS_Shape& edge) {
    if (!active_)
        return;
    const document::FeatureId id{body};
    std::optional<PickedEdge> picked;
    if (const auto named = session_.filletEdge(id, edge))
        if (const auto owner = session_.resultBody(id))
            picked = PickedEdge{id, *owner, *named};
    // On the preview only the original's edges count; the copies cannot carry their axis.
    if (!picked && previewSession_ && previewFeature_ && id == *previewFeature_)
        if (const auto named = previewSession_->filletEdge(id, edge);
            named && named->first.instances.empty() && named->second.instances.empty())
            if (const auto owner = session_.resultBody(support_))
                picked = PickedEdge{support_, *owner, *named};
    const auto ownCopy = [&](const document::SubshapeOrigin& origin) {
        return std::ranges::any_of(
            origin.instances, [&](const auto& instance) { return instance.feature == editing_; });
    };
    if (!picked || ownCopy(picked->edge.first) || ownCopy(picked->edge.second)) {
        const auto message =
            QObject::tr("Diese Kante kann nicht Richtung oder Achse sein; wähle eine Kante des "
                        "Modells.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    edge_ = std::move(picked);
    panel_.chooseOption("axis", edgeChoice()); // previews through choiceChanged
}

document::PatternParameters PatternToolController::parameters(document::BodyId body) const {
    document::PatternParameters result{body,      kind_,
                                       {},        std::nullopt,
                                       reversed_, static_cast<std::uint32_t>(count_),
                                       spacing_,  kernel::Radians{angleDegrees_ * degree},
                                       mode_};
    if (axisIndex_ < baseAxisCount) {
        result.axis.baseAxis = static_cast<document::BaseAxis>(axisIndex_ + 1);
    } else if (axisIndex_ < edgeChoice()) {
        result.datumAxis = datumAxes_[static_cast<std::size_t>(axisIndex_ - baseAxisCount)].id;
    } else if (edge_) {
        result.axis.body = edge_->body;
        result.axis.edge = edge_->edge;
        result.axis.edge->id = 1;
    }
    return result;
}

QString PatternToolController::guidance() const {
    if (!hasAxis())
        return QObject::tr("Klicke eine gerade oder kreisförmige Kante an.");
    const QString copies =
        kind_ == document::PatternKind::linear
            ? QObject::tr("%1 Exemplare im Abstand von %2 mm")
                  .arg(static_cast<int>(count_))
                  .arg(spacing_)
            : QObject::tr("%1 Exemplare über %2°").arg(static_cast<int>(count_)).arg(angleDegrees_);
    return mode_ == document::PatternMode::join
               ? QObject::tr("%1, mit dem Körper vereinigt.").arg(copies)
               : QObject::tr("%1, jede Kopie ein eigener Körper.").arg(copies);
}

void PatternToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
}

application::Action PatternToolController::action() const {
    if (editing_ && stored_)
        return application::EditPatternParametersAction{*editing_, parameters(stored_->body)};
    application::PatternInput input;
    input.kind = kind_;
    input.reversed = reversed_;
    input.count = static_cast<std::uint32_t>(count_);
    input.spacing = spacing_;
    input.angle = angleDegrees_ * degree;
    input.mode = mode_;
    if (axisIndex_ < baseAxisCount) {
        input.baseAxis = static_cast<document::BaseAxis>(axisIndex_ + 1);
    } else if (axisIndex_ < edgeChoice()) {
        input.datumAxis = datumAxes_[static_cast<std::size_t>(axisIndex_ - baseAxisCount)].id;
    } else if (edge_) {
        input.edgeSupport = edge_->support;
        input.edge = edge_->edge;
    }
    return application::CreatePatternAction{support_, input};
}

bool PatternToolController::unchanged() const {
    return stored_ && hasAxis() && *stored_ == parameters(stored_->body);
}

void PatternToolController::preview() {
    if (!active_)
        return;
    panel_.setGuidance(guidance());
    if (!hasAxis() || unchanged()) {
        panel_.setAcceptEnabled(hasAxis());
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
    previewFeature_ = editing_ ? editing_
                      : candidate.result.createdFeatures.empty()
                          ? std::nullopt
                          : std::optional{candidate.result.createdFeatures.front()};
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

void PatternToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (!hasAxis())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_, keep_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Muster geändert") : QObject::tr("Muster angelegt"));
}

void PatternToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void PatternToolController::finish() {
    active_ = false;
    viewport_.setEdgePicking(false);
    viewport_.setHighlightedEdges({});
    edge_.reset();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewSession_.reset();
    previewFeature_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
