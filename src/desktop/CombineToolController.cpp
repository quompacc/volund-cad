// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CombineToolController.hpp"

#include "CommandFeedback.hpp"
#include "ShellActions.hpp"
#include "SuccessorErrors.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureDependencies.hpp>

#include <QObject>

#include <algorithm>
#include <utility>

namespace voelund::desktop {
namespace {

int operationIndex(document::CombineOperation operation) { return static_cast<int>(operation) - 1; }

} // namespace

CombineToolController::CombineToolController(application::CadSession& session, ToolPanelHost& panel,
                                             std::function<void()> sceneChanged,
                                             std::function<void(const QString&)> statusChanged)
    : session_(session), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void CombineToolController::setPreviewHandler(
    std::function<void(const std::vector<features::EvaluatedBody>*)> handler) {
    previewChanged_ = std::move(handler);
}

void CombineToolController::begin(document::FeatureId target) {
    const auto body = session_.resultBody(target);
    if (!body) {
        statusChanged_(QObject::tr("Wähle zuerst den Körper, der das Ergebnis aufnimmt."));
        return;
    }
    target_ = target;
    targetBody_ = *body;
    tools_.clear();
    operation_ = document::CombineOperation::join;
    keepTools_ = false;
    editing_.reset();
    stored_.reset();
    open();
}

void CombineToolController::beginEdit(document::FeatureId combine) {
    const auto snapshot = session_.identifiedSnapshot().snapshot();
    const auto& values = snapshot.combineParameters(combine);
    target_.reset();
    targetBody_ = values.target;
    tools_.clear();
    // Edited combinations name bodies only; the results they had then do not matter.
    for (const auto tool : values.tools)
        tools_.emplace_back(tool, document::FeatureId{tool.value()});
    operation_ = values.operation;
    keepTools_ = values.keepTools;
    editing_ = combine;
    stored_ = values;
    open();
}

void CombineToolController::open() {
    active_ = true;
    stamp_ = session_.sceneStatus().document;
    panel_.begin(ToolPanelDefinition{
        editing_ ? QObject::tr("Kombinieren bearbeiten") : QObject::tr("Kombinieren"),
        guidance(),
        {},
        {{"operation",
          QObject::tr("Operation"),
          {QObject::tr("Vereinigen"), QObject::tr("Abziehen"), QObject::tr("Schnitt")},
          operationIndex(operation_)},
         {"tools",
          QObject::tr("Werkzeugkörper"),
          {QObject::tr("Aufnehmen"), QObject::tr("Behalten")},
          keepTools_ ? 1 : 0}},
        {{"clear_tools", QObject::tr("Werkzeugkörper zurücksetzen"), QObject::tr("Kombinieren")}},
        {},
        [this](const std::string& id, int index) {
            if (id == "operation")
                operation_ = static_cast<document::CombineOperation>(index + 1);
            else if (id == "tools")
                keepTools_ = index == 1;
            preview();
        },
        [this](const std::string& id) {
            if (id != "clear_tools")
                return;
            tools_.clear();
            preview();
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        {}});
    preview();
    statusChanged_(QObject::tr("Kombinieren: Werkzeugkörper anklicken"));
}

void CombineToolController::picked(document::FeatureId body) {
    if (!active_)
        return;
    // The previewed result is the target itself.
    if (previewFeature_ && body == *previewFeature_)
        return;
    const auto picked = session_.resultBody(body);
    if (!picked)
        return;
    if (*picked == targetBody_) {
        const auto message = QObject::tr("Das ist der Zielkörper. Klicke einen anderen Körper "
                                         "als Werkzeug an.");
        panel_.setGuidance(message);
        statusChanged_(message);
        return;
    }
    const auto known = std::ranges::find(tools_, *picked,
                                         &std::pair<document::BodyId, document::FeatureId>::first);
    if (known != tools_.end())
        tools_.erase(known);
    else
        tools_.emplace_back(*picked, body);
    preview();
}

QString CombineToolController::guidance() const {
    if (tools_.empty())
        return QObject::tr("Klicke die Körper an, die mit dem gewählten Körper kombiniert "
                           "werden. Ein zweiter Klick nimmt einen Körper wieder heraus.");
    return QObject::tr("%1 Werkzeugkörper. Weitere anklicken oder mit OK übernehmen.")
        .arg(tools_.size());
}

void CombineToolController::showModel() {
    if (previewShown_ && previewChanged_)
        previewChanged_(nullptr);
    previewShown_ = false;
    previewFeature_.reset();
}

document::CombineParameters CombineToolController::parameters() const {
    document::CombineParameters result{targetBody_, {}, operation_, keepTools_};
    for (const auto& tool : tools_)
        result.tools.push_back(tool.first);
    return result;
}

application::Action CombineToolController::action() const {
    if (editing_)
        return application::EditCombineParametersAction{*editing_, parameters()};
    std::vector<document::FeatureId> tools;
    for (const auto& tool : tools_)
        tools.push_back(tool.second);
    return application::CreateCombineAction{*target_, std::move(tools), operation_, keepTools_};
}

bool CombineToolController::unchanged() const { return stored_ && *stored_ == parameters(); }

void CombineToolController::preview() {
    if (!active_)
        return;
    if (tools_.empty() || unchanged()) {
        panel_.setAcceptEnabled(unchanged());
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
    previewFeature_ = editing_ ? editing_
                      : candidate.result.createdFeatures.empty()
                          ? std::nullopt
                          : std::optional{candidate.result.createdFeatures.front()};
    if (previewChanged_) {
        previewChanged_(&candidate.bodies);
        previewShown_ = true;
    }
}

void CombineToolController::accept() {
    if (unchanged()) {
        finish();
        return;
    }
    if (tools_.empty())
        return;
    if (!runCommand([&] { return runAction(session_, action(), stamp_, keep_); },
                    [this](const QString& message) {
                        panel_.setGuidance(message);
                        statusChanged_(message);
                    }))
        return;
    const bool edited = editing_.has_value();
    finish();
    statusChanged_(edited ? QObject::tr("Kombination geändert") : QObject::tr("Körper kombiniert"));
}

void CombineToolController::cancel() {
    if (!active_)
        return;
    finish();
    statusChanged_(QObject::tr("Werkzeug abgebrochen"));
}

void CombineToolController::finish() {
    active_ = false;
    tools_.clear();
    target_.reset();
    editing_.reset();
    stored_.reset();
    previewShown_ = false;
    previewFeature_.reset();
    panel_.end();
    sceneChanged_();
}

} // namespace voelund::desktop
