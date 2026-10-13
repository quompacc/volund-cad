#pragma once
#include <QObject>
#include <QString>
#include <exception>
#include <voelund/application/CommandResult.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace voelund::desktop {
template <class Execute, class Report> bool guardCommand(Execute execute, Report report) {
    try {
        return execute();
    } catch (const Standard_Failure& error) {
        report(QObject::tr("Vorgang fehlgeschlagen: %1")
                   .arg(QString::fromStdString(kernel::occtFailureDiagnostic(error))));
    } catch (const std::exception& error) {
        report(QObject::tr("Vorgang fehlgeschlagen: %1").arg(QString::fromUtf8(error.what())));
    }
    return false;
}

// M03: what a refused join, cut or intersect means, in the user's words; the technical
// message (with body and regions) follows. Empty for other codes.
inline QString booleanFailureText(const std::string& code) {
    if (code == "join-disjoint")
        return QObject::tr("Vereinigen: Das Werkzeug berührt den Zielkörper nicht. Ein "
                           "getrennter Körper entsteht nur mit „Neuer Körper“.");
    if (code == "cut-no-material")
        return QObject::tr("Abziehen: Das Werkzeug trifft den Zielkörper nicht oder berührt "
                           "ihn nur; es würde nichts entfernt.");
    if (code == "cut-whole-body")
        return QObject::tr("Abziehen: Das Werkzeug würde den ganzen Zielkörper entfernen.");
    if (code == "intersect-empty")
        return QObject::tr("Schneiden: Werkzeug und Zielkörper haben kein gemeinsames "
                           "Volumen; das Ergebnis wäre leer.");
    // M05: a fillet names the edges at fault (EvaluationIssue::elements are edge numbers).
    if (code == "fillet-failed")
        return QObject::tr("Verrunden: Diese Kanten lassen sich mit dem Radius nicht "
                           "verrunden; der Radius ist zu groß für die angrenzenden Flächen.");
    if (code == "fillet-edge-missing")
        return QObject::tr("Verrunden: Eine verrundete Kante gibt es so nicht mehr, ihre "
                           "Flächen wurden entfernt oder berühren sich nicht mehr.");
    // M07: the same for a chamfer.
    if (code == "chamfer-failed")
        return QObject::tr("Fase: Diese Kanten lassen sich mit dem Abstand nicht anfasen; "
                           "der Abstand ist zu groß für die angrenzenden Flächen.");
    if (code == "chamfer-edge-missing")
        return QObject::tr("Fase: Eine angefaste Kante gibt es so nicht mehr, ihre Flächen "
                           "wurden entfernt oder berühren sich nicht mehr.");
    // M08: a hole whose face is gone.
    if (code == "hole-face-missing")
        return QObject::tr("Bohrung: Die Fläche der Bohrung gibt es so nicht mehr. Wähle die "
                           "Bohrung neu auf einer Fläche des Körpers.");
    // M04: an invalid region names its contour; the shell marks the elements red.
    if (code == "region-needs-repair")
        return QObject::tr("Bereich: Die Kontur dieses Bereichs wurde in der Skizze geändert "
                           "(Element entfernt, geteilt oder verbunden). Die rot markierten "
                           "Elemente gehören dazu; wähle die Bereiche neu.");
    if (code == "region-open")
        return QObject::tr("Bereich: Die Kontur ist nicht geschlossen. Prüfe die rot "
                           "markierten Elemente auf Lücken.");
    if (code == "region-overlap")
        return QObject::tr("Bereiche: Die gewählten Bereiche überlappen sich entlang einer "
                           "Kante; wähle sie einzeln.");
    return {};
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
        const QString explained = booleanFailureText(issue.code);
        const QString technical = QString::fromStdString(issue.message);
        message += QObject::tr("\nFeature %1: %2")
                       .arg(issue.feature.value())
                       .arg(explained.isEmpty() ? technical
                                                : explained + QStringLiteral(" (") + technical +
                                                      QStringLiteral(")"));
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
            if (result.status() == CommandStatus::committed)
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
