# 11 – Native KI-Konstruktion, FEM und Optimierung

Nutzerentscheidung vom 17.09.2026; [ADR 0010](../adr/0010-native-actions-and-analysis-extension.md).

**Umstellung vom 04.10.2026 (Nutzerentscheidung):** AI02 folgt direkt auf R10b (G2), AI03
direkt auf AI02 und hängt nicht mehr an L10. Die Regel „AI02 vor U04“ war mit der neuen
Oberfläche (UX01, U04 abgenommen) übersprungen worden. AI02 schließt die Lücke, bevor
U07–U10 weitere Befehle hinzufügen. AI03 bleibt G5, damit G4 ohne KI und Netz abnehmbar
bleibt. Anbieter für AI03 zunächst Claude per API mit austauschbarem Adapter; ChatGPT
ist ausdrücklich möglich. Zuschnitt von AI02: [AI02-plan.md](AI02-plan.md).
Ziel ist direkte modellbezogene Konstruktion über Auswahl und Sprache, mit normal
weiterbearbeitbaren Features. Ein MCP-Server allein erfüllt dieses Ziel nicht.

## Jetzt, im CAD-Workflow und später

| Zeitpunkt | Verbindlicher Umfang | Bewusst zurückgestellt |
|---|---|---|
| Jetzt: AI01 | Gemeinsamer Aktions-/Kontextvertrag; vorhandene Grundlagen prüfen; R05 fortsetzen | Neue Runtime, KI-Anbieter, FEM-Bibliotheken, spekulative Dateifelder |
| G2: AI02, direkt nach R10b | Vorhandene CAD-Aktionen mit expliziten Zielen, Standprüfung und gemeinsamen Ergebnissen | Neue Modellierungsfunktionen, generischer Workflowinterpreter |
| Direkt nach AI02: AI03 | Integrierter nativer Assistent, Auswahlkontext, Vorschau und bearbeitbare Änderungen | FEM und eigenes Modelltraining |
| Optional parallel: AI04 | MCP-Adapter auf derselben Schnittstelle | Eigenständige MCP-Fachlogik oder Skriptpflicht |
| Nach v1: FEM01–03 | Begrenzte lineare Statik samt Vernetzung, Ergebnissen und Qualitätsnachweis | Kontakt, Plastizität, CFD, Dynamik, vollständiger FreeCAD-FEM-Umfang |
| Danach: OPT01 | Parameter vorhandener Features optimieren | Veränderung der Materialtopologie |
| Später: OPT02–04 | Topologieoptimierung und gesonderte CAD-Rückführung | Beliebige organische Konstruktion ohne Last-/Fertigungsvertrag |

Die G5-Zweige starten nach ihren eigenen Voraussetzungen; sie warten nicht auf alle
X01–X20. KI und FEM können unabhängig geliefert werden. G4 bleibt ohne Netz-, KI-
oder Solverzugang abnehmbar. Aufgeschobene Funktionen erscheinen nicht als aktive Werkzeuge.

## Gemeinsamer Aktionsvertrag (AI01)

Ein Request beschreibt `requestId`, `documentId`, `expectedStamp` (Sitzungsgeneration
und Revision), Aktionsart, explizite Ziel-IDs/Referenzen und typisierte Parameter.
Das ist ein Fachvertrag, noch kein veröffentlichtes Wire-Schema. Ein äußerer Adapter
serialisiert ihn versioniert; rohe OCCT-Objekte gehören nicht in dieses Schema.

- Kontextabfragen liefern Dokumentstand, Einheiten, Body-/Featurestruktur, verfügbare
  Aktionen, Parameter und aktuelle Fehler-/Referenzzustände. Große Geometrie wird
  gezielt abgefragt. Bilder ergänzen den Kontext, ersetzen keine Referenzidentität.
- Die aktuelle GUI-Auswahl darf einen Request vorbereiten, aber während seiner
  Ausführung niemals ein anderes Ziel bestimmen. Mehrdeutige Auswahl wird geklärt.
- Validierung vor Mutation: Namespace, Dokumentstand, Existenz, Fähigkeit, Einheiten,
  endliche Werte und Referenzauflösung. Veralteter Request erzeugt keinen Undo-Eintrag.
- Vorschau und mehrteilige Vorschläge arbeiten auf isoliertem Kandidaten. Eine
  bestätigte zusammenhängende Änderung hat genau einen Commit/Undo; Abbruch keinen.
  Zwischenschritte dürfen alternativ nur als ausdrücklich getrennte Aktionen erscheinen.
- Modellantworten erzeugen Aktionsvorschläge. Dateiinhalte, Namen und Kommentare sind
  Daten und können keine Ausführungsregeln oder Zugriffsrechte ändern. Dateizugriff,
  externe Übertragung und Providerkonfiguration bleiben an der äußeren Anwendungsgrenze.
- Veröffentlichung erfolgt serialisiert auf dem Session-Eigentümerthread nach erneuter
  Standprüfung. Wiederholte Request-ID darf eine Mutation nicht doppelt ausführen;
  geänderter Inhalt unter derselben ID wird abgewiesen. Kein direktes Document-Mutieren
  von Modell-/Solver-Workerthreads und kein zweiter veränderlicher Featurebaum.
- Resultat enthält Status, erzeugte/geänderte IDs, neuen Stand und strukturierte
  Diagnosen. Bestehende CommandResult-Semantik bleibt erhalten; keine Erfolgsmeldung
  auf Basis lediglich sichtbarer, aber veralteter Geometrie.

AI02 liefert zunächst Aufrufe innerhalb des Prozesses und deterministische native
Tests. Einheiten wie Kräfte, Materialkennwerte und Spannungen werden erst in FEM01
konkretisiert; die CAD-Konvention mm/Radiant bleibt jetzt unverändert.

## AI01 – Erweiterungsgrenzen und unmittelbare Grundlagen festlegen

- **Voraussetzungen:** D07, D11, R02.
- **Status:** done als Vertrags-/Planungspaket; [Nachweis](evidence/AI01.md).
- **Auftrag:** vorhandene Identität, Revision, Transaktionen und Referenzen prüfen;
  unmittelbare Integrationspflichten von späteren Adaptern und numerischen Modulen trennen.
- **Abnahme:** ADR 0010, K12/K13, ausführbare Folgepakete und konsistenter Abhängigkeitsgraph.
- **Grenze:** Kein Nachweis einer implementierten KI-/FEM-Schnittstelle.

## AI02 – Gemeinsame zielgerichtete CAD-Aktionen im Anwendungskern

- **Voraussetzungen:** AI01, R10, U02. Ursprünglich vor U04; seit 04.10.2026 direkt nach R10b.
- **Einstieg:** `CadSession.hpp`, `CommandResult.hpp`, `SceneStatus.hpp`,
  `CadSessionTransactions.cpp`, `CommandRegistry.cpp`; vorhandene Dokumenttransaktionen.
- **Auftrag:** konkrete Unterpakete für Request/Context, zielgerichtete Mutationen,
  Kandidat/Commit und Shell-Anbindung definieren. Vorhandenen Umfang (Parameteränderung,
  Skizze, Profilauswahl, Extrusion) ohne neue UI-Fachlogik anbinden. Gemeinsame
  Fähigkeitsprüfung; Legacy-Selected-Aufrufe delegieren, soweit migriert.
- **Abnahme:** A-ACTION-01; GUI und direkter Aufruf liefern gleiche Fachresultate;
  veraltete/fremde Ziele, Wiederholung, Abbruch und Batchfehler lassen Zustand konsistent.
- **Grenze:** Kein Netzwerk, Modell-SDK, Skriptinterpreter oder FEM. Größere Umsetzung
  vor Beginn gemäß Paketgrenze in Unter-IDs mit eigenen Nachweisen teilen.

## AI03 – Integrierten nativen Konstruktionsassistenten liefern

- **Voraussetzungen:** AI02 (L10 entfällt seit 04.10.2026).
- **Auftrag:** austauschbaren Modelladapter und native Qt-Bedienung spezifizieren;
  danach Auswahl/Frage → Aktionsvorschlag → Vorschau → Commit → Ergebnis verbinden.
  Erster Fall: bestehende Referenzplatte bearbeiten, Bohrungsmaße ändern und Rippe
  mit vorhandenen Features ergänzen. Unklare Bezüge gezielt klären.
- **Abnahme:** A-AI-01; reproduzierbare Adapterfixtures plus separat protokollierter
  echter Modelllauf. Ergebnisse bleiben normale editierbare Features. Abbruch,
  Netzfehler und parallele Nutzeränderung sind geprüft. Providerzugang erst hier nötig.
- **Grenze:** Kein eigenes Training, kein beliebiger generierter Programmcode;
  keine Behauptung, freie Sprache könne jede Konstruktion zuverlässig lösen.

## AI04 – Optionalen MCP-Zugang bereitstellen

- **Voraussetzungen:** AI02, L10; unabhängig von AI03/FEM.
- **Auftrag:** versionierte Tools/Kontextabfragen auf AI02 abbilden; lokale Verbindung,
  Sitzungsbindung und tatsächlichen Clientnachweis spezifizieren und umsetzen.
- **Abnahme:** A-ACTION-01 auch über Transport; gleicher Request erzeugt gleiche
  Ergebnisse und Fehler. Reconnect/Wiederholung erzeugt keine doppelte Änderung.
- **Grenze:** Kein zweiter CAD-Kern und keine Voraussetzung für den nativen Assistenten.

## FEM01 – Statikvertrag und Wiederverwendung nachweisen

- **Voraussetzungen:** AI01, L10.
- **Auftrag:** lineare isotrope Elastizität mit kleinen Verformungen für einen
  Volumenkörper spezifizieren. Material, fixierte Flächen, Kraft/Druck, konsistente
  Einheiten und abgeleitete Netzgruppen definieren. CalculiX/Gmsh gegen Alternativen
  als externe Prozesse prüfen; Format-/Resultatbausteine aus FreeCAD einzeln auditieren.
  Commit, Lizenz, Weitergabepflichten und Abhängigkeiten je Übernahme dokumentieren.
  Installation, Abbruch und Referenzrechnung auf Windows und FreeBSD nachweisen.
- **Abnahme:** begrenzter Solver-/Mesher-Spike, Modul-/Dateivertrag, Sollfälle und
  Unterpakete für FEM02/FEM03; konkrete Dependency-ADR vor Produkteinbindung.
- **Grenze:** Kein FreeCAD-/Python-Laufzeitimport, keine komplette Workbench-Portierung;
  noch keine produktive FEM-Zusage. Kandidatenauswahl ist keine Bibliotheksfreigabe.

## FEM02 – Nativen Analysefall und Berechnung liefern

- **Voraussetzungen:** FEM01.
- **Auftrag:** Analysefall speichern, Geometrie-/Material-/Lastreferenzen verwalten,
  Netz erzeugen, Solver starten/abbrechen und Ergebnisse importieren. Eigene immutable
  Eingabe-ID umfasst CAD-Stand, Analyseparameter, Netz- und Solverkonfiguration.
  Analyseänderungen verwenden normale Transaktionen; Ergebnisse sind abgeleitete Artefakte.
- **Abnahme:** A-FEM-01 einschließlich Save/Open, veralteter Referenzen, Prozessfehler,
  Abbruch und unveränderter CAD-Geometrie. Ergebnis eines alten Jobs überschreibt kein neues.
- **Grenze:** Nur in FEM01 definierte Statik; keine Optimierung oder automatische CAD-Mutation.

## FEM03 – Ergebnisdarstellung und numerische Qualität abnehmen

- **Voraussetzungen:** FEM02.
- **Auftrag:** Verformung, Verschiebung und Spannungen nativ darstellen; Skalierung,
  Einheiten, Netzqualität und Aktualität zeigen. Konvergenz und Gleichgewicht prüfen;
  Singularitäten von belastbaren Vergleichsgrößen unterscheiden.
- **Abnahme:** A-FEM-01 auf beiden Plattformen, eigene Bildprüfung und dokumentierte
  Konvergenz an analytisch vergleichbarem Stab/Balken sowie einem Halter.
- **Grenze:** Farbkarte oder erfolgreicher Prozess allein ist kein physikalischer Nachweis.

## OPT01 – Vorhandene CAD-Parameter optimieren

- **Voraussetzungen:** AI02, FEM03.
- **Auftrag:** freigegebene Parameter, Grenzen, Ziel und Nebenbedingungen bestimmen;
  Varianten isoliert rechnen, Masse/Verformung vergleichen, Rechenbudget und Abbruch
  vorsehen. Gewählte Variante als eine nachvollziehbare CAD-Änderung übernehmen.
- **Abnahme:** A-OPT-01 mit gültigen/ungültigen Varianten, unerreichbarem Ziel,
  Reproduzierbarkeit und Undo. KI kann Aufgabe erklären; Optimierer liefert Messwerte.
- **Grenze:** Keine neue Materialtopologie, keine Garantie eines globalen Optimums.

## OPT02 – Topologie- und Fertigungsvertrag experimentell bestimmen

- **Voraussetzungen:** FEM03.
- **Auftrag:** einen begrenzten 3D-Lastfall mit Design-/Erhaltungs-/Sperrbereichen,
  Materialbudget, Mindeststrukturgröße und genau einem Fertigungsverfahren wählen.
  Bestehenden Optimierer samt Lizenzen und Plattformweg evaluieren; Rechenbudget,
  Konvergenz-/Fehlermatrix und Ergebnisrepräsentation festlegen.
- **Abnahme:** reproduzierbares kleines Experiment und ausführbare Unterpakete;
  klarer Unterschied zwischen Dichtefeld, Mesh und editierbarem CAD-Solid.
- **Grenze:** Eigenes KI-Training, universelle Herstellbarkeit und automatische BRep-
  Rekonstruktion bleiben außerhalb dieses Entscheidungspakets.

## OPT03 – Begrenzte organische Formoptimierung liefern

- **Voraussetzungen:** OPT02.
- **Auftrag:** beschlossenen Optimierer an Analysefälle anbinden; Varianten mit
  Zielerreichung und Erhaltungsbereichen darstellen, abbrechen und reproduzieren.
- **Abnahme:** A-TOPO-01; Ergebnis erneut analysieren, Volumen und Nebenbedingungen
  prüfen. Unzulässige/abgebrochene Ergebnisse ersetzen das Ausgangsteil nicht.
- **Grenze:** Ergebnis zunächst ausdrücklich Optimierungsfeld/-mesh, keine erfundene Featurehistorie.

## OPT04 – Optimierungsergebnis in nutzbare CAD-Geometrie überführen

- **Voraussetzungen:** OPT03.
- **Auftrag:** einen begrenzten Rückführungsweg spezifizieren und umsetzen; Glättung,
  BRep-Rekonstruktion oder geführte Neukonstruktion vergleichen. Erhaltene Anschlüsse,
  gültige Solids, Abweichung und nachträgliche Bearbeitbarkeit nachweisen.
- **Abnahme:** A-TOPO-01 einschließlich erneuter FEM-Bewertung der tatsächlich
  zurückgeführten Geometrie und STEP-Austausch. Näherungsfehler sichtbar dokumentieren.
- **Grenze:** Kein Versprechen verlustfreier universeller Mesh-zu-Featurebaum-Konvertierung.

## Quellen für die spätere Komponentenprüfung

Recherche 17.09.2026; Links sind Ausgangspunkte, noch keine gepinnten Übernahmen:
[FreeCAD FEM](https://github.com/FreeCAD/FreeCAD/tree/main/src/Mod/Fem),
[CalculiX](https://www.calculix.de/), [Gmsh](https://gmsh.info/).
Bei Umsetzung Versionen/Commits und tatsächliche Lizenzdateien erneut prüfen.
