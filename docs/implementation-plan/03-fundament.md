# 03 – Fundament: Prüfungen, Dokument, Referenzen und Regionen

Aktueller Paketstatus und Evidenz stehen im Manifest. A01 ist abgenommen; die übrige Umsetzung folgt den Abhängigkeiten. Existierende Pfade sind Einstiegspunkte, keine Erlaubnis zu großflächigen Nebenänderungen. A03 kann Dateien aufteilen; danach nach den genannten Verantwortlichkeiten suchen. Die verbindlichen Szenario-IDs stehen in Kapitel 08. Plattformpakete P01–P04 sind in Kapitel 06 beschrieben und werden früh ausgeführt.

**Prüfung je Paket:** zuerst neuer/veränderter fachlicher Test, dann relevante CTest-Labels; bei Dokument-/Geometrieänderungen zusätzlich die bestehenden Kernel-, Dokument-, Constraint- und CadSession-Tests. Ab A04 gelten Format/Analyse/Schichten/Quellgrenzen. Plattformpflichten nicht durch einen Windows-Lauf ersetzen.

## A01 – Source-Limit-Prüfung gegen leere Eingaben absichern

- **Voraussetzungen:** keine; Basisstand aus Kapitel 01 prüfen.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tools/cmake/CheckSourceLimits.cmake`, `tests/CMakeLists.txt`, `CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Quellpfad als einen korrekt gequoteten CMake-Parameter übergeben; eingebettete literal Quotes entfernen, nicht beliebige Pfade umdeuten.
  2. Vor GLOB auf existierendes Verzeichnis prüfen; Anzahl geprüfter Dateien ausgeben und null Kandidaten als Fehler behandeln.
  3. CMake-Script-Fixtures für gültigen Pfad, Leerzeichen/Umlaut, fehlenden Pfad, literal Quotes und 601-Zeilen-Datei ergänzen.
- **Abnahme:** Der derzeitige Repositorycheck muss zunächst die vier F01-Verstöße melden. Gültige kleine Fixture besteht; alle ungültigen Eingaben scheitern mit verständlichem Grund. CTest und direkter Aufruf liefern dasselbe Ergebnis.
- **Grenze:** Keine Grenzwertanhebung und kein Ausschluss der vier Dateien.

## A02 – Verbindlichen Prüfungsumfang definieren

- **Voraussetzungen:** A01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tools/cmake/CheckSourceLimits.cmake`, `.clang-tidy`, `.clang-format`, `docs/architecture-and-code-contract.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Eigene Quellen, Header, Tests, Buildskripte und eigene Kompatibilitätsschichten inventarisieren; unveränderten Upstream/Frozen-Spike exakt ausnehmen.
  2. Maschinenlesbare Prüfungsregeln für 600 Zeilen, 80 Funktionszeilen, zyklomatische Komplexität 10, Verschachtelung 4, Parameter 5 entwerfen.
  3. Geeignetes vorhandenes Clang-Werkzeug für echte zyklomatische Messung prüfen; keine kognitive Metrik unter falschem Namen verwenden. Fehlendes Werkzeug muss im Pflichtmodus scheitern.
- **Abnahme:** Eine absichtlich verletzende Mini-Fixture pro Regel wird erkannt; saubere Fixture besteht. Ausnahmen haben Pfad, Grund und Geltungsbereich. Entscheidung über Werkzeug mit Version und Lizenz ist dokumentiert.
- **Grenze:** Keine neue Dependency ohne die bestehende ADR-Regel; keine Automatik, die Code durch Umformatieren fachlich verändert.

## A03 – Vier übergroße Dateien fachlich aufteilen

- **Voraussetzungen:** A01. Die konkreten Größenverstöße sind damit zuverlässig bekannt; der CFG-Prüfer aus A02 ist keine technische Voraussetzung für eine verhaltenserhaltende Aufteilung. A02/A04 bleiben für das Gesamtgate erforderlich.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/DocumentSerializer.cpp`, `src/viewport/CadViewport.cpp`, `src/viewport/CadViewportQtOpenGl.cpp`, `tests/integration/CadSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Serializer in Versions-/Feature-/Constraint-Codec-Verantwortlichkeiten zerlegen, öffentliche Semantik erhalten.
  2. Viewport gemeinsame Hilfsverantwortlichkeiten vorbereiten und backendbezogene Teile trennen; endgültiger Lebenszyklus folgt P02.
  3. CadSession-Tests nach Persistenz, Historie und Auswertung aufteilen; CMake-Registrierung vollständig halten.
- **Abnahme:** Alle vier Quellen und alle neu entstandenen Dateien bestehen A01; alle bisherigen Testfälle laufen weiterhin. Keine gelöschten Szenarien und keine funktionale UI-Änderung.
- **Grenze:** In drei aufeinanderfolgende Refactor-Unterpakete A03a/b/c aufteilen, falls mehr als fünf produktive Dateien je Änderung nötig sind.

## A04 – Format, statische Analyse und Schichtenprüfung ausführbar machen

- **Voraussetzungen:** A02, A03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `CMakeLists.txt`, `tools/cmake/VoelundWarnings.cmake`, `.clang-tidy`, `.clang-format`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Explizite format-check und static-analysis Ziele hinzufügen; Analyse erfasst absolute Headerpfade und eigene Adapter.
  2. Include-/Linkregeln aus K01 in einen kleinen nativen/CMake-Prüfer übersetzen; verbotenen UI-Include im Document als Fixture prüfen.
  3. Bestehende Verstöße inventarisieren, nach Verantwortlichkeit beheben und Pflichtlauf erst ohne stilles Baseline-Verstecken grün setzen.
- **Abnahme:** Formatverletzung, verbotene Schichtkante und ausgeschalteter Analysepfad machen die jeweilige Pflichtprüfung rot. Keine Änderung an Upstream. Vorgeschlagene Zielnamen und lokale Befehle sind dokumentiert.
- **Grenze:** Kein vollständiger Quellbaum-Reformat in einem Featurecommit; nötige Altlasten separat mit Paket-IDs abarbeiten.

## A05 – Teststruktur und Regressionsevidenz aufbauen

- **Voraussetzungen:** A01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tests/CMakeLists.txt`, `tests/regression/README.md`, `tests/differential/README.md`, `tests/integration/README.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Kernel-, Dokument-, Anwendungs-, UI-, Plattform- und Regressionstests mit CTest-Labels ordnen.
  2. Kleine gemeinsame native Testhilfen für temporäre Dateien, geometrische Toleranzvergleiche und strukturierte Fehler bereitstellen, sofern mehrfach benötigt.
  3. Regressionsmetadaten mit Fehler-ID, reproduzierendem Eingabezustand und erwarteter Invariante festlegen; Ausgangsbeispiele F01 und Save vorbereiten.
- **Abnahme:** Filter laufen mit mindestens einem erwarteten Test; leerer Pflichtfilter scheitert. Fixturepfade funktionieren unabhängig vom aktuellen Arbeitsverzeichnis. Kein Python im Testpfad.
- **Grenze:** Differentialtests nicht von einer lokal installierten FreeCAD/Python-Laufzeit abhängig machen; Referenzdaten separat provenance-geprüft importieren.

## A06 – Kernel-Fehlergrenze und Eingabebasis absichern

- **Voraussetzungen:** A05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/kernel/OcctCompatibility.hpp`, `src/kernel/ShapeFactory.cpp`, `src/kernel/ShapeOperations.cpp`, `tests/unit/ShapeFactoryTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Kleinen typisierten Kernel-Fehler mit Operationskennung und Diagnose einführen; Standard_Failure versionskompatibel übersetzen.
  2. Nichtendliche/degenerierte Eingaben vor OCCT ablehnen und Shape-Validierung am Operationsergebnis anbieten.
  3. Einheitliche öffentliche Fehlerrückgabe oder klar dokumentierte interne Ausnahmegrenze etablieren; keine Ausnahme aus dem späteren Qt-Callback entweichen lassen.
- **Abnahme:** Ungültige Dimension, Nullachse und absichtlich scheiternde Operation ergeben prüfbare Fehler und keinen Crash. Gültige vorhandene Primitive bleiben gleich. Tests später auf OCCT 7.9.3 und 8.0.1 ausführen.
- **Grenze:** Keine pauschale catch-all-Erfolgsmeldung; kein Umstellen aller Anwendungstypen in diesem Paket.

## A07 – Reproduzierbare Build-Einstiege herstellen

- **Voraussetzungen:** A01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `CMakePresets.json`, `tools/setup-windows.ps1`, `tools/setup-linux.sh`, `tools/setup-freebsd.sh`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Windows-ASCII-Pfadstrategie mit geprüftem Projektmarker und Kollisionsschutz automatisieren; existierendes fremdes Laufwerk nicht übernehmen.
  2. Presets eindeutig benennen: Qt-OpenGL-Produktweg und gegebenenfalls expliziter Native-Diagnoseweg; Cache-/Dependency-Versionen protokollieren.
  3. Linux-Distro/vcpkg-Widerspruch entscheiden, FreeBSD Eigen/Boost-Voraussetzungen berichtigen und frischen Setup-Befehl dokumentieren.
- **Abnahme:** Neuer Buildordner auf Windows lässt sich ohne handbearbeiteten Cache konfigurieren. Unicode-Quellpfad funktioniert direkt oder über sichtbar dokumentiertes geprüftes Mapping. Falsches Mapping wird abgewiesen. Plattformen ohne Host werden nicht als getestet markiert.
- **Grenze:** Kein unbegründetes Dependency-Upgrade; vorhandene installierte Tools nicht löschen.

## A08 – Aussagekräftige Desktop-Testbasis schaffen

- **Voraussetzungen:** A05, A07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-cad/main.cpp`, `apps/voelund-ui-preview/main.cpp`, `tests/CMakeLists.txt`, `src/viewport/CadViewportQtOpenGl.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Start-Smoke muss Eventloop, Rendering-Readiness und tatsächlich gewählten QPA-/Backendpfad prüfen.
  2. Testbare UI-Objektnamen und einen kleinen Qt-Testtreiber für Klick/Key/Parameteränderung definieren; Fachlogik nicht nur vor Fenstererstellung aufrufen.
  3. Für Logs Qt-, OCCT-, Backend-, GL-, DPI- und Betriebssystemversion erfassen, ohne benutzerspezifische Dateiinhalte zu loggen.
- **Abnahme:** Ein absichtlich nicht initialisierter Viewport darf den Smoke nicht bestehen. Produkt-Smoke zeigt tatsächlich gerendertes Modell; Skizzen-/Extrude-End-to-End kommt mit U11. Native-Wayland-Gate prüft QPA explizit. Zusätzlich A-VIS-01 vorbereiten: reproduzierbare Aufnahme mit Umgebung/Buildzustand und erfassten Darstellungsebenen; der Agent betrachtet Testbilder selbst und dokumentiert konkrete Befunde.
- **Grenze:** Keine privaten UI-Testzweige, die produktive Handler umgehen.

## D01 – Atomaren Dateiersatz als kleinen Plattformdienst bereitstellen

- **Voraussetzungen:** A05, A06, P04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/platform/README.md`, `src/application/CadSession.cpp`, `src/application/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. AtomicFileStore mit write-to-sibling-temp und betriebssystemspezifischem replace definieren; Pfadbesitz und Cleanup klar halten.
  2. Schreiben, Flush, Close, Ersetzen getrennt fehlerinjizierbar machen; kollisionsfreie temporäre Namen und passende Unicode-Pfade verwenden.
  3. Windows- und POSIX-Garantien inklusive Fehler bei Lock, Berechtigung und Dateisystemgrenze dokumentieren.
- **Abnahme:** Fixture ersetzt eine vorhandene Datei vollständig; jeder injizierte Fehler vor erfolgreichem Replace erhält deren Bytes. Gesperrtes Ziel produziert Fehler und keine falsche Erfolgsmeldung. Fremde Tempdatei bleibt unberührt.
- **Grenze:** Keine Autosave-Funktion und kein neues Dateiformat.

## D02 – Dokumentparser und Altdaten-Fixtures absichern

- **Voraussetzungen:** A05, A03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/DocumentSerializer.cpp`, `src/document/SketchConstraintSerialization.cpp`, `tests/unit/ModelDocumentTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Kleine versionierte Fixtures für jede unterstützte Version 1–16 aus deren tatsächlichem Schema erstellen; erwartete Migration als fachliche Werte notieren.
  2. Grenzen für Dateigröße, Element-/Constraintanzahl, Stringlängen und rekursive Strukturen definieren; finite Werte und Referenzkonsistenz prüfen.
  3. Truncation, unbekannte Version, doppelte IDs und ungültige Constraintreferenzen gezielt testen.
- **Abnahme:** Alle 16 Fixtures laden mit definierten Defaultwerten; Roundtrip im aktuellen Format erhält Semantik. Beschädigte Datei verändert kein schon geöffnetes Dokument. Ressourcenlimits gelten vor unbeschränkter Allokation.
- **Grenze:** Keine stillen Heuristiken zum Reparieren beliebiger beschädigter Dateien; alte Reader nicht löschen.

## D03 – Sicheren Save/Open-Anwendungsfall anbinden

- **Voraussetzungen:** D01, D02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/desktop/DocumentActions.cpp`, `tests/integration/CadSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Save nutzt Snapshot und AtomicFileStore; Dirty/Savepfad erst nach erfolgreich abgeschlossenem Ersatz aktualisieren.
  2. Open parst in einen unabhängigen Kandidaten und tauscht das aktive Dokument erst nach erfolgreicher Prüfung.
  3. Reparierbare Featurefehler von defektem Dateiformat unterscheiden; fehlerhafte parametrische Dokumente speicherbar halten.
- **Abnahme:** A-SAVE-01/02 und A-IO-01 bestehen: bisherige Datei und aktives Dokument bleiben bei Fehler erhalten, Save-As-Fehler ändert keinen Pfad, Folgefehler können gespeichert und erneut angezeigt werden.
- **Grenze:** Kein vollständiger Recovery-/Autosave-Workflow, der folgt L05.

## D04 – Befehlsresultat und Transaktionsvertrag festschreiben

- **Voraussetzungen:** A06, D03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/application/CadSession.hpp`, `include/voelund/document/ModelDocument.hpp`, `include/voelund/features/FeatureGraph.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. K03/K05 als kleine Headerentwürfe für CommandResult, CommandTransaction und Auswertungszustände konkretisieren.
  2. Zustandsübergänge für NewFeature, EditFeature mit Folgefehlern, Cancel, Undo und Save schriftlich festlegen.
  3. Zwei native Vertragsfixtures gegen einen minimalen In-memory-Adapter bauen; keine zweite produktive Dokumentimplementierung einführen.
- **Abnahme:** Prüfbare API unterscheidet Rejected, Committed und CommittedWithErrors; ein zusammengesetzter Befehl besitzt genau eine Commitgrenze. Architekturprüfung bestätigt Eigentum und Migration der alten bool-Rückgaben.
- **Grenze:** Entscheidungspaket: noch nicht sämtliche Call-Sites umstellen.

## D05 – Eine DocumentTransaction implementieren

- **Voraussetzungen:** D04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/ModelDocument.cpp`, `include/voelund/document/ModelDocument.hpp`, `tests/unit/ModelDocumentTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Änderungsaufzeichnung an expliziter äußerer Transaktion bündeln; interne Featuremutatoren erzeugen keine separaten Benutzerschritte.
  2. Rollback stellt IDs, Parameter, History-Cursor und Revision vollständig wieder her.
  3. Allokierende noexcept-Pfade bereinigen; Undo/Redo auf Wiederaufzeichnung und konsistentes Verhalten bei Fehlern prüfen.
- **Abnahme:** A-TXN-01/02 bestehen; Abbruch nach erster von zwei Teiländerungen hinterlässt byte-/semantisch gleichen Snapshot. Undo/Redo behält IDs, keine ID-Kollision nach Abbruch.
- **Grenze:** OCAF-Backendwechsel folgt separat; API jetzt so kapseln, dass D12 sie weiterverwenden kann.

## D06 – CadSession-Befehle auf einzelne Transaktionen umstellen

- **Voraussetzungen:** D05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/application/CadSessionSketchProfiles.cpp`, `tests/integration/CadSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Primitive, kombinierte SketchExtrusion und Parameteränderungen über die gemeinsame Commitgrenze führen.
  2. Bestehenden Zwei-Undo-Test gezielt durch die fachliche Ein-Aktion-Erwartung ersetzen und getrennte bestätigte Werkzeuge separat testen.
  3. Dirty-Zustand an Save-Revision binden; Preview und Selection dürfen ihn nicht setzen.
- **Abnahme:** AddSketchExtrusion → einmal Undo stellt den vollständigen Ausgangszustand her; Redo reproduziert beide internen Features. Save→Edit→Undo zur Save-Revision ist clean; Viewportinteraktion bleibt clean.
- **Grenze:** Nicht nebenbei Parameter-/Geometriesemantik neuer Featuretypen ändern.

## D07 – Fehlgeschlagenes Recompute und Solve korrekt weiterreichen

- **Voraussetzungen:** D04, D06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/application/SketchEditorSession.cpp`, `src/features/FeatureGraph.cpp`, `include/voelund/features/SketchConstraintSolver.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Verworfene Recompute-/SetParameter-Ergebnisse durch eindeutige Resultate ersetzen.
  2. Failed von Conflicting/UnderConstrained trennen; Accept bei numerischem Fehler sperren, Editor offenhalten.
  3. Letzte gültige Szene mit Revision und stale-Status kennzeichnen; explizites Übernehmen reparierbarer bestehender Modelle unterstützen.
- **Abnahme:** Injizierter Solver-/Kernel-Fehler führt weder zu Success noch zu unbemerktem Editorende. Die A-ERR-01-Fundamentstufe aus Kapitel 08 besteht; unabhängige Zweige folgen R01, gemeinsame Shell U10, Timeline M15 und produktiver Export/Gesamtnachweis M17/M18. Diese späteren Nachweise bleiben offen.
- **Grenze:** Keine automatische Parameterkorrektur ohne sichtbare Benutzeraktion.

## D08 – OCAF-Backend und gemeinsame Persistenzversion nachweisen

- **Voraussetzungen:** D04, A07, P04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/CMakeLists.txt`, `src/kernel/CMakeLists.txt`, `tools/build-freebsd-occt.sh`, `docs/dependencies.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Isolierten C++-Nachweis mit TDocStd, Standardattributen, TNaming und nativen Binärtreibern erstellen; notwendige Toolkit-Liste aus tatsächlichen Installationen ableiten.
  2. Dasselbe Minimaldokument zwischen OCCT 7.9.3 und 8.0.1 in beide Richtungen übertragen; Attribute, IDs, BRep und Undo prüfen.
  3. ADR mit eindeutigem Ergebnis gemäß K08 schreiben: native gemeinsame Schreibversion oder vorerst neutraler Snapshot-Codec; öffentliche Dokument-API und private Labelstruktur festhalten.
- **Abnahme:** Evidenz enthält beide Versionen, benötigte Buildmodule, Schreib-/Lesematrix und vollständigen Resultatvergleich. Ohne verfügbaren zweiten Build bleibt Persistenzumschaltung blockiert; kein behaupteter Kompatibilitätserfolg.
- **Grenze:** Kein Rewrite. Keine neue Kompressions-/Serialisierungsbibliothek. Experimentcode nicht ungeprüft als Produktionsbackend übernehmen.

## D09 – Legacy-Migrationsschnittstelle vom Produktcodec trennen

- **Umsetzung/Nachweis:** [D09](evidence/D09.md); Legacy-Import, Snapshot und Importbericht einschließlich Session-Lebensdauer.

- **Voraussetzungen:** D02, D08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/DocumentSerializer.hpp`, `src/document/DocumentSerializer.cpp`, `src/document/SketchConstraintSerialization.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Reader 1–16 hinter LegacyDocumentImporter bündeln; neutrales validiertes Snapshotmodell definieren.
  2. Produktstore entscheidet eindeutig anhand von Formatkennung/Schemaversion; unbekannte Zukunftsformate werden explizit abgewiesen.
  3. Migrationsbericht benennt alte Index-/Flächenreferenzen, die noch in R06 aufgelöst werden müssen.
- **Abnahme:** Alle D02-Fixtures bleiben lesbar; Import verändert Quelldateien nicht. Unbekanntes Format fällt nicht auf einen zufälligen älteren Parser zurück. Migration ist bei Wiederholung deterministisch.
- **Grenze:** Keine automatische Vergabe beliebiger Ersatzflächen.

## D10 – Geometrische Grundwerte und toleranzbewusste Validierung vereinheitlichen

- **Abgeschlossen:** [D10](evidence/D10.md); D10a beseitigt Richtungsüberlauf, D10b1 schützt nicht kanonische Altachsen; [D10b2b](evidence/D10b2b.md) schließt die Winkelmigration ab. [D10c](evidence/D10c.md) und [D10d](evidence/D10d.md) sind numerisch geprüft. Die [Abschlussprüfung](evidence/D10-review.md) fand einen ID-Überlauf; [D10e](evidence/D10e.md) behebt ihn mit 49/49 FreeBSD/Sway und 53/53 Windows. Formeller Abschluss auf ausdrücklichen Nutzerauftrag vom 15.09.2026.

- **Voraussetzungen:** A06, D04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/SketchGeometry.cpp`, `include/voelund/document/SketchGeometry.hpp`, `src/features/SketchEvaluation.cpp`, `src/kernel/ShapeOperations.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Einheiten, finite Werte, Ebenenbasis, Toleranzrollen und Größenlimits in testbaren Value-Types/Validatoren festlegen.
  2. Ebenen vor Speicherung normalisieren oder nichtorthonormale Eingabe mit genauem Fehler ablehnen; beide Wege nicht mischen.
  3. Dimensionen, Transformationen und Kernelanschlüsse an denselben Vertrag anbinden, ohne Picking-Toleranz als Modellmaß zu verwenden.
- **Abnahme:** A-NUM-01 besteht mit kleinen/großen Körpern, schiefen Achsen, NaN/Inf und fast verbundenen Endpunkten. Roundtrip ändert Skala nicht; kein pauschaler unkommentierter Überstand bleibt fachliche Voraussetzung.
- **Grenze:** Keine neue globale Toleranzvariable; Änderung der vorhandenen Maße nur mit expliziter Migration.

## D11 – Dokument-, Körper- und Featureidentität trennen

- **Fortschritt:** [D11a](evidence/D11a.md) führt getrennte ID-Typen und eine deterministische Legacy-Körperzuordnung ein. [D11b](evidence/D11b.md) sichert Feature-ID-Vergabe über Undo und trennt Inhaltsgleichheit vom Vergabezähler. [D11c](evidence/D11c.md) bindet die rekonstruierbare Körperidentität an die produktive Auswertung an und trennt sie vom Shape-Erzeuger. [D11d](evidence/D11d.md) prüft native ID-Feldkodierung und isolierte OCAF-Laufzeit. [D11e](evidence/D11e.md) implementiert Zufallsvergabe und Kopie-/Revisions-/Duplikatsemantik am unveränderlichen Snapshot-Übergang. [D11f](evidence/D11f.md) ergänzt den nativen Dokumentkopf. [D11g](evidence/D11g.md) prüft die vollständige native Box-Abbildung einschließlich Snapshot-Parität und Plattformaustausch. [D11h](evidence/D11h.md) ergänzt Zylinder aller Modi, native Eingabereferenzen und eine von Körpergruppen unabhängige Feature-Reihenfolge. [D11i](evidence/D11i.md) ergänzt native Skizzenebenen, Elementgeometrie, IDs und Stützreferenzen. [D11j](evidence/D11j.md) ergänzt alle 26 nativen Constraintarten mit Elementreferenzen und Solver-Parität. [D11k](evidence/D11k.md) ergänzt native Extrusionen und geordnete Profilauswahl. [D11l](evidence/D11l.md) gleicht die Modellfelder ab und prüft einen gemeinsamen Identitätstest auf beiden Plattformen samt Dateiaustausch. [D11m](evidence/D11m.md) bindet den Identitätslebenszyklus an CadSession (Windows und FreeBSD geprüft). [D11n](evidence/D11n.md) ergänzt die Desktop-Duplikataktion mit Save/Cancel/Fehlerpfaden, auf Windows und FreeBSD geprüft. Die [D11-Abnahmematrix](evidence/D11-review.md) schließt die ursprünglichen Identitätskriterien ab. Dauerhafte native Produktidentität folgt in D13, der Autoritätswechsel in D12; D11 ist `done`.
- **Voraussetzungen:** D05, D09, D10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/FeatureId.hpp`, `include/voelund/document/ModelDocument.hpp`, `src/document/ModelDocument.cpp`, `src/features/FeatureGraph.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Typisierte DocumentId/BodyId und stabile Feature-ID-Zuordnung einführen; Migration bestehender Ein-Körper-Ketten eindeutig definieren.
  2. Feature-Output referenziert Körper und Erzeuger separat; Displayname/Dateipfad sind keine IDs.
  3. ID-Erzeugung, Kopie versus Revision und Nichtwiederverwendung dokumentieren; Überlauf vor Mutation abfangen.
- **Abnahme:** Umbenennen, Save-As und weiteres Feature erhalten Körperidentität. Zwei Körper bleiben nach Reorder getrennt. Legacy-Kette erhält reproduzierbare Zuordnung; ungültige IDs werden abgewiesen.
- **Grenze:** Vault-Netzwerkprotokoll und Assembly-Instanz-ID folgen später.

## D12 – OCAF als gekapselte Dokumentautorität einsetzen

- **Fortschritt:** [D12a](evidence/D12a.md) kapselt Modellzustand und History privat. [D12b](evidence/D12b.md) bindet Auswertung und Writer an unveränderliche Snapshots und liefert eigene UI-Datenwerte. [D12c](evidence/D12c.md) ergänzt den isolierten privaten OCAF-Zustand mit vorbereiteter Veröffentlichung und Historie vollständiger OCAF-Stände. [D12d](evidence/D12d.md) integriert diesen Zustand als Produkt-Modellautorität samt Transaktionen/Undo/Redo und Dokument-ID. D12 bleibt bis D12e `in-progress`; die bestätigte [Aufteilung D12a–D12e](D12-plan.md) trennt isolierte OCAF-Vorbereitung (D12c), Produktumschaltung (D12d) und Gesamtabnahme (D12e).

- **Voraussetzungen:** D08, D09, D11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/document/ModelDocument.cpp`, `src/document/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Private OCAF-Implementierung aus dem bestätigten D08-Entwurf einführen; Standardattribute halten Parameter, IDs und Referenzen.
  2. DTOs ausschließlich als immutable Snapshots erzeugen; vorhandene mutable Vektorautorität entfernen, sobald Adapterparität bewiesen ist.
  3. Transaktionen/Undo mit OCAF verbinden und UI-/App-Header frei von TDF-Labels halten.
- **Abnahme:** D05/D06-Historientests laufen gegen den neuen Backend; Parameter und IDs nach Undo/Redo identisch. Kein Codepfad schreibt parallel in zwei Modellquellen. OCAF-spezifische Tests nach Verantwortung splitten.
- **Grenze:** Bei Umfangüberschreitung Featurefamilien einzeln migrieren; Adapter bleibt währenddessen ausdrücklich nur eine Autorität pro Featurezustand.

## D13 – Gewählten Produktstore integrieren

- **Voraussetzungen:** D03, D09, D12.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/DocumentSerializer.cpp`, `src/application/CadSession.cpp`, `src/document/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Den in D08 gewählten Store an AtomicFileStore anschließen; Root-Schema und Anwendungsversion sauber unterscheiden.
  2. Parameter, IDs, Fehler-/Suppressionzustand und später persistierbare Quellshapes gemäß Schema speichern; abgeleitete Anzeige nicht zur Wahrheit machen.
  3. Save-Revision und Save-As über OCAF-Transaktionen hinweg stabil halten.
- **Abnahme:** Neue Datei lässt sich auf beiden Ziel-OCCT-Versionen fachlich identisch laden; A-SAVE-01/02 bleibt gültig. Eine Datei mit Folgefehlern bleibt editierbar und wird nicht als kaputt verworfen.
- **Grenze:** Nur den gewählten Schreibweg produktiv anbieten; alternative D08-Prototypen nicht dauerhaft mitschleppen.

## D14 – Migration und Dokumentkern abschließend abnehmen

- **Voraussetzungen:** D07, D13.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tests/unit/ModelDocumentTests.cpp`, `tests/integration/CadSessionTests.cpp`, `docs/sketch-data-model.md`, `docs/development-status.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Versionen 1–16 durch Import→neues Save→Open→Edit→Undo führen; semantische Erwartungsdaten getrennt von Serialisierungsbytes vergleichen.
  2. Schema-/Transaktions-/Fehlerregeln aktualisieren und veraltete Implementierungsbehauptungen berichtigen.
  3. Migration ohne vorhandenes Modell, mit Lock, unbekannter Version und beschädigter Referenz prüfen.
- **Abnahme:** A-MIG-01, A-TXN-01/02, A-SAVE-01/02 bestehen. Mindestens ein Fixture je historischem Versionssprung belegt dessen Default-/Constraintsemantik. G1-Bericht nennt noch offene Plattformnachweise.
- **Grenze:** Kein Entfernen alter Fixtures nach erfolgreicher Migration.

## R01 – Feature-Abhängigkeiten als echten DAG auswerten

- **Voraussetzungen:** D11, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/features/FeatureGraph.hpp`, `src/features/FeatureGraph.cpp`, `src/application/CadSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Explizite Inputs/Outputs mit Feature-/Bodyreferenzen aus vorhandenen Featuretypen ableiten; Toposort und Zyklenerkennung implementieren.
  2. Änderungen markieren nur transitive Nachfolger dirty; unabhängige Körper bleiben gültig.
  3. Auswertungsergebnis pro Knoten mit konsistenter Dokumentrevision veröffentlichen; lineare Komplettfehler-Markierung ablösen.
- **Abnahme:** Zwei unabhängige Körper: Fehler in einem Zweig blockiert nur dessen Nachfolger. Diamant-Abhängigkeit wird einmal pro benötigtem Zustand ausgewertet. Zyklusänderung wird atomar abgelehnt. A-DAG-01 besteht.
- **Grenze:** Noch kein Threadpool; deterministische synchrone Auswertung zuerst.

## R02 – SubshapeRef-Vertrag und Referenzfixtures festlegen

- **Voraussetzungen:** R01, D08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/features/SketchSupportResolver.hpp`, `include/voelund/document/SketchGeometry.hpp`, `src/features/SketchSupportResolver.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. K06 in Datenstruktur und Resolver-Resultate übersetzen; Beispielschlüssel für Boxfläche, Extrusionsdeckfläche und Konturkante definieren.
  2. Fixtures für zwei koplanare Flächen, Split/Merge, Löschung, Rotation und Boolean-History anlegen.
  3. Persistenz-/Migrationseinträge und Reparatur-Commandentwurf festhalten; geometrische Fallbacks ausdrücklich klassifizieren.
- **Abnahme:** Architekturprüfung kann für jeden Fixturefall vor Implementierung Resolved/Missing/Ambiguous vorhersagen. Kein TopExp-Index und keine normale Distanz ist alleinige Identität.
- **Grenze:** Entscheidungspaket; keine universelle automatische Naming-Garantie behaupten.

## R03 – Kernel-Operationshistorie verfügbar machen

- **Voraussetzungen:** R02, A06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeFactory.cpp`, `src/kernel/ShapeOperations.cpp`, `include/voelund/kernel/ShapeOperations.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Geometrieergebnis mit Herkunftsinformationen für vorhandene Primitive und Boolean-Operationen ergänzen.
  2. OCCT Generated/Modified/Deleted in eigene begrenzte History-DTOs abbilden; OCCT-Objektlebensdauer kapseln.
  3. Fehlende oder mehrdeutige History nicht durch Traversalposition erfinden.
- **Abnahme:** Bekannte Boxfläche und Kante nach einfachem Cut korrekt verfolgen; gelöschte Fläche wird Missing, gesplittete Fläche nach Vertrag eindeutig oder Ambiguous. Beide OCCT-Versionen vergleichen.
- **Grenze:** Fillet-/Chamfer-History erst mit den betreffenden Operationen ergänzen.

## R04 – Stützflächen über stabile Referenzen auflösen

- **Voraussetzungen:** R03, D10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchSupportResolver.cpp`, `src/features/SketchEvaluation.cpp`, `src/application/CadSessionSketchProfiles.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Heuristische Normalen-/Nähe-Suche im regulären Pfad durch SubshapeRef ersetzen.
  2. Skizzenbasis aus aktueller referenzierter Fläche samt Transformation ableiten; Translation und Rotation tragen.
  3. Unaufgelöste Stützfläche als lokalisierten Featurefehler veröffentlichen; alte Ebene nur gekennzeichnet für Reparaturvorschau behalten.
- **Abnahme:** A-REF-01/02 besteht: zwei parallele Flächen werden nicht vertauscht; Rotation trägt die Skizze korrekt; Löschung bleibt sichtbar reparaturbedürftig.
- **Grenze:** Keine stillschweigende Wiederverwendung der alten Ebene als gültiges Ergebnis.

## R05 – Projizierte Kanten von Segmentindizes lösen

- **Voraussetzungen:** R04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchSupportResolver.cpp`, `include/voelund/document/SketchGeometry.hpp`, `src/viewport/SketchInteraction.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Projizierte Geometrie referenziert stabile Quellkante und Kurvenparameter statt diskretisierten Traversalindex.
  2. Fachliche Auflösung vollständig über Feature-/Application-Projektion beziehen; doppelte Viewport-Heuristik entfernen.
  3. Nichtlineare Kurven bei Anzeige tessellieren, Identität jedoch an analytischer Kurve halten.
- **Abnahme:** Tessellierungsdichte ändern beeinflusst Constraints nicht. Neue Kante vor der referenzierten Kante fügt keine falsche Zuordnung ein. Kurven-Endpunkt/Parameter bleiben nach Recompute konsistent.
- **Grenze:** Keine allgemeine Projektionsfläche jenseits planarer Skizzen in v1.

## R06 – Legacy-Referenzen migrieren und bewusste Reparatur ermöglichen

- **Voraussetzungen:** R05, D13.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/document/DocumentSerializer.cpp`, `src/application/CadSessionSketchProfiles.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Alte Supportfelder beim Import gegen unveränderte Ausgangsgeometrie auflösen und eindeutige Herkunft speichern.
  2. Mehrdeutige Fälle als MigrationNeedsRepair mit Kandidaten anbieten; Benutzerzuweisung als einzelne Transaktion speichern.
  3. Reparatur rückgängig machen und Save/Open testen.
- **Abnahme:** Alte Datei mit eindeutiger Fläche migriert; zwei gleichwertige Kandidaten erzeugen keine Zufallsauswahl. A-REF-03 besteht, inklusive Undo und erneutem Öffnen.
- **Grenze:** Keine Änderung an Originaldatei ohne Save.

## R07 – Deterministischen Konturgraphen erstellen

- **Voraussetzungen:** D10, R01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchProfiles.cpp`, `include/voelund/features/SketchProfiles.hpp`, `tests/unit/SketchProfileTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Linien/Arcs/Kreise/Ellipsen und migrierte Rechtecke in orientierte Randstücke überführen; Konstruktion ausschließen.
  2. Endpunktverbindungen toleranzbewusst bilden; unabhängig von Eingabe- und ID-Reihenfolge traversieren.
  3. Offene Enden, Verzweigungen und überlappende Dubletten mit Element-IDs diagnostizieren.
- **Abnahme:** Permutation und Umkehr gleicher Kontur liefern geometrisch gleiche geschlossene Schleifen. Offene Skizze bleibt editierbar, aber kein gültiger Materialrand. A-REG-01 Teil Konturen besteht.
- **Grenze:** Noch keine verschachtelten Faces und keine heuristische automatische Schließung.

## R08 – Schnittpunkte und verschachtelte Regionen bestimmen

- **Voraussetzungen:** R07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchProfiles.cpp`, `src/features/SketchEvaluation.cpp`, `tests/unit/SketchProfileTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Planare Schnittpunkte der v1-Kurven behandeln und Randstücke deterministisch teilen; unterstützte degenerierte Fälle ausdrücklich entscheiden.
  2. Geschlossene Schleifen nach Innen/Außen und Verschachtelung ordnen; gemeinsame Kanten und Selbstschnitte klassifizieren.
  3. Regionsresultate mit Außen- und Innenkonturen liefern; ungültige Fälle lokalisiert ablehnen.
- **Abnahme:** Zwei konzentrische Kreise ergeben Ringregion plus gegebenenfalls separat auswählbare Innenregion gemäß Vertrag; Loch wird nicht automatisch aufgefüllt. Linie/Kreis-Schnitt, Tangentialfall und Selbstschnitt sind deterministisch getestet.
- **Grenze:** Bei mehr als einer Algorithmusverantwortlichkeit R08a Schnittbildung und R08b Containment getrennt liefern; keine ungeprüfte allgemeine NURBS-Unterstützung.

## R09 – Regionidentität und Profilauswahl stabilisieren

- **Voraussetzungen:** R08, R02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/features/SketchProfiles.hpp`, `src/features/SketchProfiles.cpp`, `src/application/CadSessionSketchProfiles.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. RegionIds aus stabilen Randbezügen zuordnen und über reine Maßänderungen erhalten.
  2. Split/Merge eindeutig erkennen; alte min-Element-ID-Profile mit dokumentierter Ausgangszuordnung migrieren.
  3. Explizite Mehrfachauswahl und ungültig gewordene Profilreferenz in Application-Projektion anbieten.
- **Abnahme:** Elementumordnung verändert Auswahl nicht; gelöschter kleinster Randindex führt nicht unbemerkt zur anderen Region. Split wird nach Vertrag reparaturbedürftig. A-REG-02 besteht.
- **Grenze:** Kein automatisches Auswählen aller erkannten Flächen als Ersatz für Benutzerabsicht.

## R10 – Regionen zu validen Kernel-Faces überführen

- **Voraussetzungen:** R08, R09, A06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchEvaluation.cpp`, `src/kernel/ShapeFactory.cpp`, `tests/unit/SketchProfileTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Orientierte Außen-/Innenwires korrekt in OCCT-Faces überführen; Shape-Validität und Flächeninhalt prüfen.
  2. Fehler bis Region-/Element-IDs zurückführen; partielle ungültige Regionen nicht als volle erfolgreiche Skizze ausgeben.
  3. Analytische Fixtures und Toleranzmatrix für kleine/große Flächen ergänzen.
- **Abnahme:** A-REG-01 vollständig besteht: Ringfläche π(R²−r²), Inseln, mehrere getrennte Regionen und invertierte Eingabereihenfolge. Invalides Face liefert Fehler statt leeres Erfolgsshape.
- **Grenze:** Keine Extrude-UI; diese nutzt später dieselben Regionresultate.
