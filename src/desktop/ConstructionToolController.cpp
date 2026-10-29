// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ConstructionToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <algorithm>
#include <exception>
#include <utility>

namespace voelund::desktop {

ConstructionToolController::ConstructionToolController(
    application::CadSession& session, ToolPanelHost& panel, std::function<void()> sceneChanged,
    std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void ConstructionToolController::setPreviewHandler(PreviewHandler handler) {
    previewChanged_ = std::move(handler);
}

std::optional<ConstructionToolController::Face>
ConstructionToolController::faceOf(const viewport::PlanarFaceSelection& face) const {
    const document::FeatureId support{face.bodyId};
    try {
        return Face{support, session_.faceReference(support, face.selectedFace).origin};
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

// The construction points before the edited feature (all of them for a new one).
void ConstructionToolController::collectPoints() {
    points_.clear();
    for (const auto& feature : session_.features()) {
        if (editing_ && feature.id == *editing_)
            break;
        if (feature.kind == document::FeatureKind::datumPoint)
            points_.push_back({QString::fromStdString(feature.name), feature.id});
    }
}

void ConstructionToolController::start(Kind kind) {
    kind_ = kind;
    editing_.reset();
    stored_.reset();
    second_.reset();
    chosen_ = {0, 1, 2};
    position_ = {0.0, 0.0, 0.0};
    spot_ = document::EdgeSpot::start;
    collectPoints();
}

void ConstructionToolController::beginMidplane(const viewport::PlanarFaceSelection& face) {
    start(Kind::midplane);
    first_ = faceOf(face);
    if (!first_) {
        statusChanged_(QObject::tr("Wähle eine ebene Fläche eines Körpers."));
        return;
    }
    open();
}

void ConstructionToolController::beginPlaneThroughPoints() {
    start(Kind::planeThroughPoints);
    if (points_.size() < 3) {
        statusChanged_(QObject::tr("Eine Ebene durch Punkte braucht drei Bezugspunkte."));
        return;
    }
    open();
}

void ConstructionToolController::beginPoint(std::optional<document::FeatureId> support,
                                            std::optional<document::FilletEdge> edge) {
    start(Kind::point);
    edgeSupport_ = edge ? support : std::nullopt;
    edge_ = edge;
    open();
}

void ConstructionToolController::beginEdit(document::FeatureId feature) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& parameters = snapshot.featureParameters(feature);
    start(Kind::point);
    editing_ = feature;
    stored_ = parameters;
    collectPoints();
    if (const auto* point = std::get_if<document::DatumPointParameters>(&parameters)) {
        position_ = point->position.value_or(std::array{0.0, 0.0, 0.0});
        edge_ = point->edge;
        spot_ = point->spot;
    } else if (const auto& plane = std::get<document::DatumPlaneParameters>(parameters);
               !plane.points.empty()) {
        kind_ = Kind::planeThroughPoints;
        for (std::size_t index = 0; index < 3; ++index) {
            const auto found = std::ranges::find(points_, plane.points[index],
                                                 &decltype(points_)::value_type::second);
            chosen_[index] = static_cast<int>(found - points_.begin());
        }
    } else {
        kind_ = Kind::midplane;
    }
    open();
}

ToolPanelDefinition ConstructionToolController::definition() {
    ToolPanelDefinition result{{},
                               guidance(),
                               {},
                               {},
                               {},
                               [this](const std::string& id, double value) {
                                   if (id == "x" || id == "y" || id == "z")
                                       position_[static_cast<std::size_t>(id[0] - 'x')] = value;
                                   preview();
                               },
                               [this](const std::string& id, int index) {
                                   if (id == "spot")
                                       spot_ = static_cast<document::EdgeSpot>(index + 1);
                                   else if (id.starts_with("point"))
                                       chosen_[static_cast<std::size_t>(id[5] - '1')] = index;
                                   preview();
                               },
                               {},
                               [this]() { accept(); },
                               [this]() { cancel(); },
                               {}};
    if (kind_ == Kind::midplane) {
        result.title = QObject::tr("Mittelebene");
    } else if (kind_ == Kind::planeThroughPoints) {
        result.title = QObject::tr("Ebene durch drei Punkte");
        std::vector<QString> names;
        for (const auto& point : points_)
            names.push_back(point.first);
        for (int index = 0; index < 3; ++index)
            result.choiceParameters.push_back({"point" + std::to_string(index + 1),
                                               QObject::tr("Punkt %1").arg(index + 1), names,
                                               chosen_[static_cast<std::size_t>(index)]});
    } else if (edge_) {
        result.title = QObject::tr("Bezugspunkt");
        result.choiceParameters.push_back({"spot",
                                           QObject::tr("Lage"),
                                           {QObject::tr("Anfang"), QObject::tr("Ende"),
                                            QObject::tr("Mitte"), QObject::tr("Mittelpunkt")},
                                           static_cast<int>(spot_) - 1});
    } else {
        result.title = QObject::tr("Bezugspunkt");
        for (const auto* axis : {"x", "y", "z"})
            result.lengthParameters.push_back({axis, QString::fromLatin1(axis).toUpper(),
                                               position_[static_cast<std::size_t>(axis[0] - 'x')],
                                               -1'000'000.0, 1'000'000.0});
    }
    if (editing_)
        result.title = QObject::tr("%1 bearbeiten").arg(result.title);
    return result;
}

void ConstructionToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(definition());
    preview();
}

QString ConstructionToolController::guidance() const {
    switch (kind_) {
    case Kind::midplane:
        return second_ ? QObject::tr("Die Ebene liegt mittig zwischen den beiden Flächen.")
                       : QObject::tr("Klicke die zweite, parallele Fläche an.");
    case Kind::planeThroughPoints:
        return QObject::tr("Die Ebene geht durch die drei Punkte; der erste ist ihr Ursprung.");
    default:
        return edge_ ? QObject::tr("Der Punkt liegt auf der angeklickten Kante.")
                     : QObject::tr("Der Punkt liegt an den Koordinaten.");
    }
}

void ConstructionToolController::picked(const viewport::PlanarFaceSelection& face) {
    if (!active_ || kind_ != Kind::midplane)
        return;
    second_ = faceOf(face);
    panel_.setGuidance(guidance());
    preview();
}

// The edit of a stored element, with the panel's values.
std::optional<application::Action> ConstructionToolController::editAction() const {
    if (auto* point = std::get_if<document::DatumPointParameters>(&*stored_)) {
        auto values = *point;
        if (values.position)
            values.position = position_;
        values.spot = spot_;
        return application::EditDatumPointParametersAction{*editing_, values};
    }
    auto plane = std::get<document::DatumPlaneParameters>(*stored_);
    if (kind_ == Kind::planeThroughPoints)
        for (std::size_t index = 0; index < 3; ++index)
            plane.points[index] = points_.at(static_cast<std::size_t>(chosen_[index])).second;
    else if (second_)
        plane.secondFace = document::BodyFaceReference{
            session_.resultBody(second_->support).value_or(plane.secondFace->body),
            second_->origin};
    return application::EditDatumPlaneParametersAction{*editing_, plane};
}

std::optional<application::Action> ConstructionToolController::action() const {
    if (editing_)
        return editAction();
    if (kind_ == Kind::point)
        return application::CreateDatumPointAction{edge_ ? std::nullopt : std::optional{position_},
                                                   edgeSupport_, edge_, spot_};
    application::CreateDatumPlaneAction plane{std::nullopt, std::nullopt, std::nullopt,
                                              millimetres(0.0)};
    if (kind_ == Kind::planeThroughPoints) {
        for (const int index : chosen_)
            plane.points.push_back(points_.at(static_cast<std::size_t>(index)).second);
        return plane;
    }
    if (!first_ || !second_)
        return std::nullopt;
    plane.support = first_->support;
    plane.face = first_->origin;
    plane.secondSupport = second_->support;
    plane.secondFace = second_->origin;
    return plane;
}

void ConstructionToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewSession_.reset();
}

void ConstructionToolController::preview() {
    if (!active_)
        return;
    const auto chosen = action();
    if (!chosen) {
        panel_.setAcceptEnabled(false);
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_, application::prepareRequest(session_, *chosen, stamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed ||
                    result.reason() == application::RejectionReason::noChange;
    panel_.setAcceptEnabled(ok);
    panel_.setGuidance(ok ? guidance() : commandFeedbackMessage(result));
    if (!ok || !candidate.candidate) {
        showModel();
        return;
    }
    previewSession_ = candidate.candidate;
    if (previewChanged_) {
        previewChanged_(previewSession_.get());
        previewShown_ = true;
    }
}

void ConstructionToolController::accept() {
    const auto chosen = action();
    if (!chosen)
        return;
    // An edit that changes nothing just closes the panel.
    if (!runCommand([&] { return runAction(session_, *chosen, stamp_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    },
                    editing_.has_value()))
        return;
    finish();
    statusChanged_(QObject::tr("Konstruktionselement angelegt"));
}

void ConstructionToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void ConstructionToolController::finish() {
    active_ = false;
    editing_.reset();
    stored_.reset();
    first_.reset();
    second_.reset();
    edge_.reset();
    previewShown_ = false;
    previewSession_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
