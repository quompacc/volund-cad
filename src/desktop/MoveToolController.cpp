// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "MoveToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <numbers>
#include <string>
#include <utility>

namespace voelund::desktop {
namespace {
constexpr double degree = std::numbers::pi / 180.0;
constexpr const char* fieldIds[] = {"x", "y", "z", "rx", "ry", "rz"};
} // namespace

MoveToolController::MoveToolController(application::CadSession& session, ToolPanelHost& panel,
                                       std::function<void()> sceneChanged,
                                       std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void MoveToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

void MoveToolController::beginEdit(document::FeatureId move) {
    const auto values = session_.moveParameters(move);
    const auto& t = values.transform;
    values_ = {t.translationX,
               t.translationY,
               t.translationZ,
               t.rotationX.value() / degree,
               t.rotationY.value() / degree,
               t.rotationZ.value() / degree};
    editing_ = move;
    stored_ = values;
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    constexpr double range = 1'000'000.0;
    panel_.begin(withFieldBindings(
        ToolPanelDefinition{
            QObject::tr("Bewegen bearbeiten"),
            QObject::tr("Verschiebung und Drehung um die Mitte des Körpers."),
            {{"x", QObject::tr("X"), values_[0], -range, range},
             {"y", QObject::tr("Y"), values_[1], -range, range},
             {"z", QObject::tr("Z"), values_[2], -range, range},
             {"rx", QObject::tr("Drehung X"), values_[3], -360.0, 360.0, ToolValueKind::angle},
             {"ry", QObject::tr("Drehung Y"), values_[4], -360.0, 360.0, ToolValueKind::angle},
             {"rz", QObject::tr("Drehung Z"), values_[5], -360.0, 360.0, ToolValueKind::angle}},
            {},
            {},
            [this](const std::string& id, double value) {
                for (std::size_t index = 0; index < values_.size(); ++index)
                    if (id == fieldIds[index])
                        values_[index] = value;
                preview();
            },
            [](const std::string&, int) {},
            [](const std::string&) {},
            [this]() { accept(); },
            [this]() { cancel(); },
            {}},
        editing_, [this] { stamp_ = session_.sceneStatus().document; }));
    statusChanged_(QObject::tr("Bewegen: Werte ändern"));
}

document::MoveParameters MoveToolController::parameters() const {
    return {stored_->body,
            {values_[0], values_[1], values_[2], kernel::Radians{values_[3] * degree},
             kernel::Radians{values_[4] * degree}, kernel::Radians{values_[5] * degree}},
            stored_->pivot};
}

void MoveToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
}

void MoveToolController::preview() {
    if (!active_)
        return;
    if (parameters() == *stored_) {
        panel_.setAcceptEnabled(true);
        showModel();
        return;
    }
    const auto candidate = application::previewAction(
        session_,
        application::prepareRequest(
            session_, application::EditMoveParametersAction{*editing_, parameters()}, stamp_));
    const auto& result = candidate.result.command;
    const bool ok = result.status() == application::CommandStatus::committed;
    panel_.setErrorCommitHandler({});
    panel_.setAcceptEnabled(ok);
    panel_.setGuidance(ok ? QObject::tr("Verschiebung und Drehung um die Mitte des Körpers.")
                          : commandFeedbackMessage(result));
    if (!ok) {
        offerKeepingSuccessorErrors(panel_, result, editing_, [this] {
            keep_ = application::ErrorDisposition::keepEditableErrors;
            accept();
            keep_.reset();
        });
        showModel();
        return;
    }
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

void MoveToolController::accept() {
    if (parameters() == *stored_) {
        finish();
        return;
    }
    if (!runCommand(
            [&] {
                return runAction(session_,
                                 application::EditMoveParametersAction{*editing_, parameters()},
                                 stamp_, keep_);
            },
            [this](const QString& message) {
                panel_.setGuidance(message);
                statusChanged_(message);
            }))
        return;
    finish();
    statusChanged_(QObject::tr("Bewegen geändert"));
}

void MoveToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void MoveToolController::finish() {
    active_ = false;
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
