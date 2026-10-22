// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "CommandFeedback.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CommandResult.hpp>
#include <voelund/document/FeatureId.hpp>

#include <QObject>

#include <functional>
#include <optional>
#include <utility>

namespace voelund::desktop {

// M13: an edit refused only because later features fail; the edited feature itself evaluates.
[[nodiscard]] inline bool onlySuccessorsFail(const application::CommandResult& result,
                                             document::FeatureId edited) {
    if (result.status() != application::CommandStatus::rejected ||
        result.reason() != application::RejectionReason::evaluationFailed)
        return false;
    bool later = false;
    for (const auto& issue : result.evaluation().features) {
        if (issue.status == features::EvaluationStatus::valid)
            continue;
        if (issue.feature == edited)
            return false;
        later = true;
    }
    return later;
}

// M13: while an edit fails only at later features, the panel offers "Mit Folgefehlern
// übernehmen"; `keep` then commits the same edit with those errors, which stay editable at their
// features. Nothing later is deleted. Any other result withdraws the offer.
inline void offerKeepingSuccessorErrors(ToolPanelHost& panel,
                                        const application::CommandResult& result,
                                        std::optional<document::FeatureId> edited,
                                        std::function<void()> keep) {
    if (!edited || !onlySuccessorsFail(result, *edited)) {
        panel.setErrorCommitHandler({});
        return;
    }
    panel.setGuidance(commandFeedbackMessage(result) + QLatin1Char('\n') +
                      QObject::tr("Spätere Features schlagen damit fehl. Du kannst die Änderung "
                                  "trotzdem übernehmen; sie bleiben erhalten und lassen sich "
                                  "danach reparieren."));
    panel.setErrorCommitHandler(std::move(keep));
}

} // namespace voelund::desktop
