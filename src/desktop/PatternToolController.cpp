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
                                  std::optional<document::FilletEdge> edge,
                                  std::vector<document::FeatureId> steps) {
    support_ = support;
    kind_ = kind;
    editing_.reset();
    stored_.reset();
    reversed_ = false;
    steps_ = std::move(steps);
    mode_ = steps_.empty() ? document::PatternMode::newBodies : document::PatternMode::join;
    count_ = kind == document::PatternKind::linear ? 3 : 4;
    spacing_ = 20.0;
    angleDegrees_ = 360.0;
    resetGridAndPath();
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
    skipped_ = values.skipped;
    steps_ = values.features;
    datumAxes_.clear();
    for (const auto& feature : snapshot.features()) {
        if (feature.id == pattern)
            break;
        if (feature.kind == document::FeatureKind::datumAxis)
            datumAxes_.push_back(feature);
    }
    edge_.reset();
    collectPaths(pattern);
    if (values.path) {
        takePath(values);
    } else if (values.axis.baseAxis) {
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
    takeGrid(values);
    open();
}

// N11a/N11c: a new pattern has no rows and no skipped copies; its path is the latest sketch.
void PatternToolController::resetGridAndPath() {
    grid_ = false;
    rows_ = 2;
    rowSpacing_ = 20.0;
    rowAxis_ = 1;
    rowsReversed_ = false;
    skipped_.clear();
    followPath_ = false;
    collectPaths(std::nullopt);
    pathIndex_ = paths_.empty() ? 0 : static_cast<int>(paths_.size()) - 1;
}

// N11a: a grid's rows along a base or construction axis.
void PatternToolController::takeGrid(const document::PatternParameters& values) {
    grid_ = values.second.has_value();
    if (!grid_)
        return;
    const auto& second = *values.second;
    rows_ = second.count;
    rowSpacing_ = second.spacing;
    rowsReversed_ = second.reversed;
    rowAxis_ = second.axis.baseAxis ? static_cast<int>(*second.axis.baseAxis) - 1 : 0;
    if (second.datumAxis)
        if (const auto found =
                std::ranges::find(datumAxes_, *second.datumAxis, &document::FeatureDescriptor::id);
            found != datumAxes_.end())
            rowAxis_ = baseAxisCount + static_cast<int>(found - datumAxes_.begin());
}

// N11c: every earlier sketch with drawn elements is a path, all its elements as one chain.
void PatternToolController::collectPaths(std::optional<document::FeatureId> before) {
    paths_.clear();
    for (const auto& feature : session_.features()) {
        if (before && feature.id == *before)
            break;
        if (feature.kind != document::FeatureKind::sketch)
            continue;
        document::SweepPath path{feature.id, {}};
        for (const auto& element : session_.sketchParameters(feature.id).elements.values)
            if (!element.construction)
                path.elements.push_back(element.id);
        if (!path.elements.empty())
            paths_.push_back({QString::fromStdString(feature.name), std::move(path)});
    }
}

void PatternToolController::takePath(const document::PatternParameters& values) {
    followPath_ = values.followPath;
    paths_.erase(
        std::remove_if(paths_.begin(), paths_.end(),
                       [&](const auto& path) { return path.second.sketch == values.path->sketch; }),
        paths_.end());
    paths_.push_back({QObject::tr("Gespeicherter Pfad"), *values.path});
    pathIndex_ = static_cast<int>(paths_.size()) - 1;
}

int PatternToolController::edgeChoice() const noexcept {
    return baseAxisCount + static_cast<int>(datumAxes_.size());
}

bool PatternToolController::hasAxis() const noexcept {
    if (kind_ == document::PatternKind::path)
        return !paths_.empty();
    return axisIndex_ < edgeChoice() || edge_.has_value();
}

void PatternToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(withFieldBindings(definition(), editing_,
                                   [this] { stamp_ = session_.sceneStatus().document; }));
    panel_.focusLength("count"); // W05: the main value takes the keyboard
    syncFields();
    viewport_.setEdgePicking(true);
    preview();
    highlightEdge();
    statusChanged_(QObject::tr("Muster: Achse wählen oder eine Kante anklicken"));
}

// The panel. N11a: "Raster" adds rows along a second axis; any copy can be skipped.
ToolPanelDefinition PatternToolController::definition() {
    std::vector<QString> axes{QObject::tr("X-Achse"), QObject::tr("Y-Achse"),
                              QObject::tr("Z-Achse")};
    for (const auto& feature : datumAxes_)
        axes.push_back(QString::fromStdString(feature.name));
    const std::vector<QString> rowAxes = axes;
    axes.push_back(QObject::tr("Kante (anklicken)"));
    const std::vector<QString> sides{QObject::tr("Normal"), QObject::tr("Umgekehrt")};
    std::vector<QString> pathNames;
    for (const auto& path : paths_)
        pathNames.push_back(path.first);
    return ToolPanelDefinition{
        editing_ ? QObject::tr("Muster bearbeiten") : QObject::tr("Muster"),
        guidance(),
        {{"count", QObject::tr("Anzahl"), count_, 2.0, 100.0, ToolValueKind::count},
         {"spacing", QObject::tr("Abstand"), spacing_, 0.001, 1'000'000.0},
         {"angle", QObject::tr("Winkel"), angleDegrees_, 0.001, 360.0, ToolValueKind::angle},
         {"rows", QObject::tr("Reihen"), rows_, 2.0, 50.0, ToolValueKind::count},
         {"rowSpacing", QObject::tr("Reihenabstand"), rowSpacing_, 0.001, 1'000'000.0},
         {"skip", QObject::tr("Kopie Nr."), 1.0, 1.0, 99.0, ToolValueKind::count}},
        {{"kind",
          QObject::tr("Art"),
          {QObject::tr("Linear"), QObject::tr("Kreisförmig"), QObject::tr("Raster"),
           QObject::tr("Entlang Kurve")},
          kindChoice()},
         {"path", QObject::tr("Pfad"), pathNames, pathIndex_},
         {"orientation",
          QObject::tr("Ausrichtung"),
          {QObject::tr("Gleich"), QObject::tr("Am Pfad")},
          followPath_ ? 1 : 0},
         {"axis", QObject::tr("Richtung/Achse"), axes, axisIndex_},
         {"direction", QObject::tr("Richtung"), sides, reversed_ ? 1 : 0},
         {"rowAxis", QObject::tr("Reihenrichtung"), rowAxes, rowAxis_},
         {"rowDirection", QObject::tr("Reihen"), sides, rowsReversed_ ? 1 : 0},
         {"mode",
          QObject::tr("Ergebnis"),
          {QObject::tr("Neue Körper"), QObject::tr("Vereinigen")},
          mode_ == document::PatternMode::join ? 1 : 0}},
        {{"skip", QObject::tr("Überspringen"), QObject::tr("Muster")}},
        [this](const std::string& id, double value) { changedValue(id, value); },
        [this](const std::string& id, int index) { chooseOption(id, index); },
        [this](const std::string& id) {
            if (id == "skip")
                toggleSkipped();
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        {}};
}

void PatternToolController::changedValue(const std::string& id, double value) {
    if (id == "count")
        count_ = std::round(value);
    else if (id == "spacing")
        spacing_ = value;
    else if (id == "angle")
        angleDegrees_ = value;
    else if (id == "rows")
        rows_ = std::round(value);
    else if (id == "rowSpacing")
        rowSpacing_ = value;
    else if (id == "skip")
        skipIndex_ = std::round(value);
    else
        return;
    dropSkippedBeyond();
    preview();
}

void PatternToolController::chooseOption(const std::string& id, int index) {
    if (id == "kind") {
        kind_ = index == 1   ? document::PatternKind::circular
                : index == 3 ? document::PatternKind::path
                             : document::PatternKind::linear;
        grid_ = index == 2;
        dropSkippedBeyond();
    } else {
        chooseSetting(id, index);
    }
    syncFields();
    preview();
    highlightEdge();
}

void PatternToolController::chooseSetting(const std::string& id, int index) {
    const bool second = index == 1;
    if (id == "axis")
        axisIndex_ = index;
    else if (id == "direction")
        reversed_ = second;
    else if (id == "rowAxis")
        rowAxis_ = index;
    else if (id == "rowDirection")
        rowsReversed_ = second;
    else if (id == "path")
        pathIndex_ = index;
    else if (id == "orientation")
        followPath_ = second;
    else if (id == "mode")
        mode_ = second ? document::PatternMode::join : document::PatternMode::newBodies;
}

// "Art": Linear, Kreisförmig, Raster (N11a), Entlang Kurve (N11c).
int PatternToolController::kindChoice() const noexcept {
    if (grid_)
        return 2;
    if (kind_ == document::PatternKind::path)
        return 3;
    return kind_ == document::PatternKind::circular ? 1 : 0;
}

std::uint32_t PatternToolController::instances() const noexcept {
    return static_cast<std::uint32_t>(count_) * (grid_ ? static_cast<std::uint32_t>(rows_) : 1U);
}

// N11a: "Überspringen" leaves copy "Kopie Nr." out, or brings it back.
void PatternToolController::toggleSkipped() {
    const auto index = static_cast<std::uint32_t>(skipIndex_);
    if (index == 0 || index >= instances())
        return;
    if (const auto found = std::ranges::find(skipped_, index); found != skipped_.end())
        skipped_.erase(found);
    else
        skipped_.insert(std::ranges::upper_bound(skipped_, index), index);
    preview();
}

void PatternToolController::dropSkippedBeyond() {
    std::erase_if(skipped_, [&](std::uint32_t index) { return index >= instances(); });
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

// A linear pattern has a spacing, a circular one an angle; a grid its rows too.
void PatternToolController::syncFields() {
    const bool along = kind_ == document::PatternKind::path; // N11c
    panel_.setLengthVisible("spacing", kind_ != document::PatternKind::circular);
    panel_.setChoiceVisible("axis", !along);
    panel_.setChoiceVisible("direction", !along);
    panel_.setChoiceVisible("path", along);
    panel_.setChoiceVisible("orientation", along);
    panel_.setLengthVisible("angle", kind_ == document::PatternKind::circular);
    panel_.setLengthVisible("rows", grid_);
    panel_.setLengthVisible("rowSpacing", grid_);
    panel_.setChoiceVisible("rowAxis", grid_);
    panel_.setChoiceVisible("rowDirection", grid_);
    panel_.setChoiceVisible("mode", steps_.empty()); // N11b: repeated steps always join
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
    if (kind_ == document::PatternKind::path) {
        result.path = paths_.at(static_cast<std::size_t>(pathIndex_)).second;
        result.followPath = followPath_;
    } else if (axisIndex_ < baseAxisCount) {
        result.axis.baseAxis = static_cast<document::BaseAxis>(axisIndex_ + 1);
    } else if (axisIndex_ < edgeChoice()) {
        result.datumAxis = datumAxes_[static_cast<std::size_t>(axisIndex_ - baseAxisCount)].id;
    } else if (edge_) {
        result.axis.body = edge_->body;
        result.axis.edge = edge_->edge;
        result.axis.edge->id = 1;
    }
    result.second = second();
    result.skipped = skipped_;
    result.features = steps_;
    return result;
}

std::optional<document::PatternDirection> PatternToolController::second() const {
    if (!grid_)
        return std::nullopt;
    document::PatternDirection result;
    result.count = static_cast<std::uint32_t>(rows_);
    result.spacing = rowSpacing_;
    result.reversed = rowsReversed_;
    if (rowAxis_ < baseAxisCount)
        result.axis.baseAxis = static_cast<document::BaseAxis>(rowAxis_ + 1);
    else
        result.datumAxis = datumAxes_[static_cast<std::size_t>(rowAxis_ - baseAxisCount)].id;
    return result;
}

QString PatternToolController::guidance() const {
    if (!hasAxis())
        return kind_ == document::PatternKind::path
                   ? QObject::tr("Ein Muster entlang einer Kurve braucht eine frühere Skizze.")
                   : QObject::tr("Klicke eine gerade oder kreisförmige Kante an.");
    QString copies =
        grid_ ? QObject::tr("%1 × %2 Exemplare").arg(static_cast<int>(count_)).arg(rows_)
        : kind_ != document::PatternKind::circular
            ? QObject::tr("%1 Exemplare im Abstand von %2 mm")
                  .arg(static_cast<int>(count_))
                  .arg(spacing_)
            : QObject::tr("%1 Exemplare über %2°").arg(static_cast<int>(count_)).arg(angleDegrees_);
    if (!skipped_.empty()) {
        QStringList numbers;
        for (const auto index : skipped_)
            numbers.push_back(QString::number(index));
        copies += QObject::tr(" (ohne Kopie %1)").arg(numbers.join(QStringLiteral(", ")));
    }
    if (!steps_.empty())
        return QObject::tr("%1 von %n Schritt(en).", nullptr, static_cast<int>(steps_.size()))
            .arg(copies);
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
    input.second = second();
    input.skipped = skipped_;
    input.features = steps_;
    if (kind_ == document::PatternKind::path) { // N11c
        input.path = paths_.at(static_cast<std::size_t>(pathIndex_)).second;
        input.followPath = followPath_;
    } else if (axisIndex_ < baseAxisCount) {
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
