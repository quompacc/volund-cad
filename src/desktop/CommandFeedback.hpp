// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "FeatureIssueText.hpp"
#include <QObject>
#include <QString>
#include <exception>
#include <voelund/application/CommandResult.hpp>
#include <voelund/kernel/KernelError.hpp>

namespace voelund::desktop {
template <class Execute, class Report> bool guardCommand(Execute execute, Report report) {
    try {
        bool done = false;
        kernel::translateOcctFailures(kernel::KernelOperation::evaluation,
                                      [&] { done = execute(); });
        return done;
    } catch (const kernel::KernelError& error) {
        report(
            QObject::tr("Vorgang fehlgeschlagen: %1")
                .arg(QString::fromStdString(error.operation() == kernel::KernelOperation::evaluation
                                                ? error.diagnostic()
                                                : std::string(error.what()))));
    } catch (const std::exception& error) {
        report(QObject::tr("Vorgang fehlgeschlagen: %1").arg(QString::fromUtf8(error.what())));
    }
    return false;
}

inline QString commandFeedbackMessage(const application::CommandResult& result) {
    QString message = result.status() == application::CommandStatus::committedWithErrors
                          ? QObject::tr("Änderungen mit Fehlern übernommen.")
                          : QObject::tr("Änderung abgelehnt. Das Dokument blieb unverändert.");
    if (!result.diagnostic().empty())
        message += QStringLiteral("\n") + QString::fromStdString(result.diagnostic());
    for (const auto& issue : result.evaluation().features) {
        if (issue.status != features::EvaluationStatus::failed &&
            issue.status != features::EvaluationStatus::blockedByDependency)
            continue;
        // U15: the explanation by code, with the technical message after it.
        message += QObject::tr("\nFeature %1: %2")
                       .arg(issue.feature.value())
                       .arg(issueExplanation(issue.code, issue.message));
    }

    return message;
}

// Qt callbacks consume expected rejection and contain unexpected exceptions.
// The command owns rollback; this boundary only presents the outcome.
template <class Execute, class Report>
bool runCommand(Execute execute, Report report, bool acceptNoChange = false) {
    return guardCommand(
        [&] {
            const auto result = execute();
            using application::CommandStatus;
            // M13: a commit with errors happens only when it was asked for.
            if (result.status() == CommandStatus::committed ||
                result.status() == CommandStatus::committedWithErrors)
                return true;
            if (result.status() == CommandStatus::rejected &&
                result.reason() == application::RejectionReason::noChange)
                return acceptNoChange;
            report(commandFeedbackMessage(result));
            return false;
        },
        report);
}
} // namespace voelund::desktop
