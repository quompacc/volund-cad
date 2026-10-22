// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureIssueText.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <string_view>

namespace voelund::desktop {

namespace {

struct IssueText {
    std::string_view code;
    const char* text;
};

// M03 Booleans, M05/M07 fillet and chamfer edges (EvaluationIssue::elements are edge numbers),
// M04 regions (the shell marks their elements red) and the solid and sketch features.
constexpr std::array issueTexts{
    IssueText{"join-disjoint",
              QT_TRANSLATE_NOOP("FeatureIssue", "Vereinigen: Das Werkzeug berührt den Zielkörper "
                                                "nicht. Ein getrennter Körper entsteht nur mit "
                                                "„Neuer Körper“.")},
    IssueText{"cut-no-material",
              QT_TRANSLATE_NOOP("FeatureIssue", "Abziehen: Das Werkzeug trifft den Zielkörper "
                                                "nicht oder berührt ihn nur; es würde nichts "
                                                "entfernt.")},
    IssueText{"cut-whole-body",
              QT_TRANSLATE_NOOP("FeatureIssue",
                                "Abziehen: Das Werkzeug würde den ganzen Zielkörper entfernen.")},
    IssueText{"intersect-empty",
              QT_TRANSLATE_NOOP("FeatureIssue", "Schneiden: Werkzeug und Zielkörper haben kein "
                                                "gemeinsames Volumen; das Ergebnis wäre leer.")},
    IssueText{"fillet-failed",
              QT_TRANSLATE_NOOP("FeatureIssue", "Verrunden: Diese Kanten lassen sich mit dem "
                                                "Radius nicht verrunden; der Radius ist zu groß "
                                                "für die angrenzenden Flächen.")},
    IssueText{"fillet-edge-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Verrunden: Eine verrundete Kante gibt es so "
                                                "nicht mehr, ihre Flächen wurden entfernt oder "
                                                "berühren sich nicht mehr.")},
    IssueText{"chamfer-failed",
              QT_TRANSLATE_NOOP("FeatureIssue", "Fase: Diese Kanten lassen sich mit dem Abstand "
                                                "nicht anfasen; der Abstand ist zu groß für die "
                                                "angrenzenden Flächen.")},
    IssueText{"chamfer-edge-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Fase: Eine angefaste Kante gibt es so nicht "
                                                "mehr, ihre Flächen wurden entfernt oder "
                                                "berühren sich nicht mehr.")},
    IssueText{"hole-face-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bohrung: Die Fläche der Bohrung gibt es so "
                                                "nicht mehr. Wähle die Bohrung neu auf einer "
                                                "Fläche des Körpers.")},
    IssueText{"shell-failed",
              QT_TRANSLATE_NOOP("FeatureIssue", "Wandstärke: Der Körper lässt sich mit dieser "
                                                "Dicke nicht aushöhlen; die Wände würden sich "
                                                "durchdringen.")},
    IssueText{"shell-face-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Wandstärke: Eine offene Fläche gibt es so nicht "
                                                "mehr. Wähle die Flächen neu.")},
    IssueText{
        "revolve-axis-missing",
        QT_TRANSLATE_NOOP("FeatureIssue",
                          "Drehen: Die Drehachse gibt es so nicht mehr. Wähle die Achse neu.")},
    IssueText{"helix-one-region",
              QT_TRANSLATE_NOOP("FeatureIssue", "Wendel: Eine Wendel entsteht aus genau einem "
                                                "Bereich der Skizze.")},
    IssueText{"datum-reference-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bezug: Die Fläche oder Kante, auf der die "
                                                "Bezugsebene oder -achse liegt, gibt es so nicht "
                                                "mehr.")},
    IssueText{"datum-axis-edge",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bezugsachse: Nur gerade oder kreisförmige "
                                                "Kanten ergeben eine Achse.")},
    IssueText{"combine-body-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Kombinieren: Ein beteiligter Körper hat vor der "
                                                "Kombination keinen Volumenkörper mehr.")},
    IssueText{"mirror-plane-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Spiegeln: Die Spiegelebene gibt es so nicht "
                                                "mehr. Wähle eine Grundebene, Bezugsebene oder "
                                                "ebene Fläche neu.")},
    IssueText{"mirror-body-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Spiegeln: Der Körper hat vor dem Spiegeln "
                                                "keinen Volumenkörper mehr.")},
    IssueText{"move-body-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bewegen: Der Körper hat vor dem Bewegen keinen "
                                                "Volumenkörper mehr.")},
    IssueText{"pattern-axis-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Muster: Die Richtung oder Achse gibt es so "
                                                "nicht mehr. Wähle eine Achse oder Kante neu.")},
    IssueText{"pattern-body-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Muster: Der Körper hat vor dem Muster keinen "
                                                "Volumenkörper mehr.")},
    IssueText{"draft-face-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Formschräge: Eine geneigte Fläche gibt es so "
                                                "nicht mehr. Wähle die Flächen neu.")},
    IssueText{"draft-neutral-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Formschräge: Die neutrale Fläche gibt es so "
                                                "nicht mehr. Wähle sie neu.")},
    IssueText{"draft-face-refused",
              QT_TRANSLATE_NOOP("FeatureIssue", "Formschräge: Eine der Flächen lässt sich um "
                                                "diesen Winkel nicht neigen.")},
    IssueText{"loft-section-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Ausformung: Eine Querschnittskizze fehlt oder "
                                                "hat keinen Bereich.")},
    IssueText{"loft-section-hole",
              QT_TRANSLATE_NOOP("FeatureIssue", "Ausformung: Querschnitte mit Löchern lassen "
                                                "sich nicht verbinden.")},
    IssueText{"loft-coplanar",
              QT_TRANSLATE_NOOP("FeatureIssue", "Ausformung: Zwei Querschnitte liegen in "
                                                "derselben Ebene.")},
    IssueText{"sweep-path-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Folgen: Der Pfad fehlt oder hat keine "
                                                "Elemente mehr.")},
    IssueText{"sweep-path-disconnected",
              QT_TRANSLATE_NOOP("FeatureIssue", "Folgen: Die Pfadelemente bilden keine "
                                                "zusammenhängende Kette.")},
    IssueText{"import-part-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Import: Ein importierter Körper fehlt im "
                                                "Dokument.")},
    IssueText{"region-needs-repair",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bereich: Die Kontur dieses Bereichs wurde in "
                                                "der Skizze geändert (Element entfernt, geteilt "
                                                "oder verbunden). Die rot markierten Elemente "
                                                "gehören dazu; wähle die Bereiche neu.")},
    IssueText{"region-open",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bereich: Die Kontur ist nicht geschlossen. "
                                                "Prüfe die rot markierten Elemente auf Lücken.")},
    IssueText{"region-overlap",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bereiche: Die gewählten Bereiche überlappen "
                                                "sich entlang einer Kante; wähle sie einzeln.")},
    IssueText{"profile-invalid",
              QT_TRANSLATE_NOOP("FeatureIssue", "Profil: Aus der Kontur entsteht keine gültige "
                                                "Fläche. Prüfe sie auf Lücken und "
                                                "Selbstüberschneidungen.")},
    IssueText{"sketch-unsolved",
              QT_TRANSLATE_NOOP("FeatureIssue", "Skizze: Die Bedingungen und Maße lassen sich "
                                                "nicht gleichzeitig erfüllen. Öffne die Skizze "
                                                "und löse den Widerspruch.")},
    IssueText{"sketch-support-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Skizze: Die Fläche, auf der die Skizze liegt, "
                                                "gibt es so nicht mehr.")},
    IssueText{"sketch-support-unsolved",
              QT_TRANSLATE_NOOP("FeatureIssue", "Skizze: Die Bezüge auf den Körper lassen sich "
                                                "nicht mehr erfüllen.")},
    IssueText{"sketch-plane-missing",
              QT_TRANSLATE_NOOP("FeatureIssue",
                                "Skizze: Die Bezugsebene der Skizze gibt es nicht mehr.")},
    IssueText{"sketch-needs-repair",
              QT_TRANSLATE_NOOP("FeatureIssue", "Skizze: Die Bezüge dieser älteren Skizze "
                                                "müssen einmal neu zugeordnet werden; nutze "
                                                "„Reparieren“.")},
    IssueText{"end-face-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bis Fläche: Die Endfläche gibt es so nicht "
                                                "mehr. Wähle sie neu.")},
    IssueText{"end-face-not-parallel",
              QT_TRANSLATE_NOOP("FeatureIssue",
                                "Bis Fläche: Die Endfläche muss parallel zum Profil liegen.")},
    IssueText{"end-face-behind",
              QT_TRANSLATE_NOOP("FeatureIssue", "Bis Fläche: Die Endfläche liegt nicht in "
                                                "Richtung der Extrusion.")},
    IssueText{"profile-face-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Drücken/Ziehen: Die Fläche gibt es so nicht "
                                                "mehr. Wähle sie neu.")},
    IssueText{"body-missing",
              QT_TRANSLATE_NOOP("FeatureIssue", "Der Körper dieses Schritts hat davor keinen "
                                                "Volumenkörper mehr; ein früherer Schritt ist "
                                                "fehlerhaft oder unterdrückt.")},
    IssueText{"base-body-exists",
              QT_TRANSLATE_NOOP("FeatureIssue", "Extrusion: Der Körper hat schon einen "
                                                "Volumenkörper; wähle Vereinigen, Abziehen oder "
                                                "einen neuen Körper.")},
    IssueText{"blocked-by-dependency",
              QT_TRANSLATE_NOOP("FeatureIssue", "Dieser Schritt wartet auf einen fehlerhaften "
                                                "früheren Schritt.")},
    IssueText{
        "rolled-back",
        QT_TRANSLATE_NOOP("FeatureIssue", "Dieser Schritt liegt hinter der Zurückrollen-Marke.")},
    IssueText{"stale-input",
              QT_TRANSLATE_NOOP("FeatureIssue", "Ein Eingang dieses Schritts ist noch nicht "
                                                "neu berechnet.")},
    IssueText{"needs-target",
              QT_TRANSLATE_NOOP("FeatureIssue", "Extrusion: Vereinigen und Abziehen brauchen "
                                                "einen Zielkörper.")},
};

} // namespace

QString issueText(const std::string& code) {
    const auto found = std::ranges::find(issueTexts, std::string_view(code), &IssueText::code);
    return found == issueTexts.end() ? QString{}
                                     : QCoreApplication::translate("FeatureIssue", found->text);
}

QString issueExplanation(const std::string& code, const std::string& technical) {
    const QString explained = issueText(code);
    const QString details = QString::fromStdString(technical);
    if (details.isEmpty())
        return explained;
    return (explained.isEmpty() ? QCoreApplication::translate(
                                      "FeatureIssue", "Dieser Schritt lässt sich nicht berechnen.")
                                : explained) +
           QStringLiteral(" (") + details + QStringLiteral(")");
}

} // namespace voelund::desktop
