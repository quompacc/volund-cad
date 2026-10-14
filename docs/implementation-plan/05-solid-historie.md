# 05 – Mechanisches CAD, Historie und Austausch

K02/K05/K06/K08/K10 gelten für jeden neuen Featuretyp. Ein neuer Typ braucht Parameter, validierten Evaluator, Herkunftsreferenzen, Store/Migration, Registry/Controller, Edit/Undo und mindestens einen positiven sowie einen negativen Fachtest. Die wiederkehrenden Schritte sind kein Grund, sie auszulassen oder in UI-Callbacks zu verstecken.

**Prüfweg:** native Kernel-/Featuretests, `application.recompute_history` und Nachfolger nach Testaufteilung, passende UI-Szenarien aus Kapitel08. Geometrie nicht nur auf 'Shape ungleich null' prüfen, sondern Validität, Volumen/Fläche, Lage, Körperanzahl und Referenzen.

## M01 – Körpermodell und erster unabhängiger Solid

- **Voraussetzungen:** S14, R01, D11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/application/CadSession.cpp`, `include/voelund/document/ModelDocument.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Leeres Dokument und mehrere BodyIds ohne künstliches Startfeature unterstützen.
  2. Featureinputs beziehen sich auf expliziten Zielkörper/Erzeuger; 'letztes Feature' nicht als pauschaler Zielkörper verwenden.
  3. NewBody-Ausgabe und Sichtbarkeit/Selektion von Körpern in der Projektion anbieten.
  4. Direkten Mehrkörperworkflow gemäß Nutzeranforderung vom 16.09.2026 sicherstellen:
     beliebige weitere unabhängige Körper über denselben NewBody-Ablauf, ohne
     Komponenten-/Baugruppenpflicht. Körperbereich mit individuellen Namen,
     Sichtbarkeit, Auswahl und Platzierung; kein Fehler allein durch einen zweiten Solid.
- **Abnahme:** Zwei unabhängige Körper lassen sich erstellen und gezielt verändern; dazwischen angelegte Skizze verändert Zielwahl nicht. A-BODY-01, A-BODY-03 und A-DAG-01 bestehen. Der UI-Ablauf ist ebenso Pflicht wie das interne Mehrkörpermodell.
- **Grenze:** Keine Baugruppeninstanzen; Body ist im Dokument modellierte Geometrie.

## M02 – Extrusionsparameter und Kernel-Auswertung vollständig definieren

- **Voraussetzungen:** M01, R10, R03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/features/SketchEvaluation.cpp`, `src/features/FeatureGraph.cpp`, `src/kernel/ShapeOperations.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ExtrudeSpec mit Regionreferenzen, NewBody/Join/Cut/Intersect, Zielkörper, Richtung und Blind/Reverse/Symmetric definieren.
  2. Prismenerzeugung an Kernelgrenze verschieben, positive Distanz plus explizite Richtung statt widersprüchlicher Vorzeichen verwenden.
  3. Semantische Deck-/Mantelflächenreferenzen und Operationshistory liefern; Face-/Solidvalidität vor Commit prüfen.
- **Abnahme:** Rechteckextrusion hat analytisches Volumen und richtige Orientierung auf XY/XZ/YZ. Symmetrisch verteilt Gesamtlänge entsprechend dokumentiertem Vertrag. A-EXT-01 besteht ohne vorhandenen Körper.
- **Grenze:** Taper und weitere Endbedingungen zunächst deaktiviert; keinen heimlichen Overlap als universelle Lösung.

## M03 – Join, Pocket/Cut und Intersect robust auswerten

- **Voraussetzungen:** M02, D10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `tests/integration/CadSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Operationen mit explizitem Zielkörper und validen Tool-Solids durchführen; Pocket nutzt denselben Extrusionskern mit Cut-Semantik.
  2. Berührend, überlappend, getrennt und leeres Resultat je Operation eindeutig behandeln; unbeabsichtigte Mehrkörperresultate melden oder gemäß Body-Vertrag übernehmen.
  3. Fehler mit Region-/Body-IDs an Anwendung weitergeben und History vollständig halten.
- **Abnahme:** A-EXT-02: analytischer Durchbruch und Sacktasche korrekt; getrenntes Join nach festgelegtem Vertrag abgewiesen; Intersect ohne Schnitt erklärt leeres Ergebnis. Alter Zielkörper bleibt bei Reject unverändert.
- **Grenze:** Keine versteckte automatische Änderung von NewBody zu Join oder Zielkörperwechsel.

## M04 – Produktives Extrude/Pocket-Panel und kompletter erster CAD-Fluss

- **Voraussetzungen:** M03, U12.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/SketchExtrusionToolController.cpp`, `src/desktop/ToolPanelHost.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Distanz/Richtung/Operation/Ziel/Regionauswahl wirklich an ExtrudeSpec binden; Preview nutzt denselben Evaluator wie Commit.
  2. OK erzeugt genau eine Featuretransaktion, Cancel verwirft Vorschau; Tool bei ungültiger Region erklärt betroffene Konturen.
  3. Registry-Einträge aktivieren und UI-Test um echte Extrusion/Pocket erweitern.
- **Abnahme:** A-UI-01 vollständig: neu→Skizze→Extrude→Maß ändern→Undo/Redo→Save/Open. Parameteränderung verändert Preview messbar; Cancel verändert Dokument nicht. G2-Abnahme protokollieren.
- **Grenze:** Keine ungebundenen Taper-/Direction-Felder im aktiven Panel.

## M05 – Fillet-Kerneloperation und Referenzhistory

- **Voraussetzungen:** M03, R06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `include/voelund/document/ModelDocument.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. FilletFeature mit Kantenreferenzen und Radius serialisierbar ergänzen; Kerneloperation isoliert ausführen.
  2. BRep-Validität, Radiusgrenzen und Generated/Modified/Deleted-History prüfen.
  3. Fehler auf ausgewählte Kanten zurückführen; ungültiger Radius darf Ausgangsmodell nicht beschädigen.
- **Abnahme:** A-FILLET-01: einzelner gerader Boxrand mit kleinem Radius erzeugt validen Solid und erwartete Volumenänderung. Zu großer Radius wird als Fehler behandelt. Nach Basismaßänderung Referenz korrekt oder explizit reparaturbedürftig.
- **Grenze:** Keine Variable-Radius-/allgemeine Flächenverrundung in diesem Paket.

## M06 – Fillet-Bearbeitung über generischen Toolhost

- **Voraussetzungen:** M05, M04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/ToolPanelHost.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/viewport/CadViewportQtOpenGl.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Kantenselektionsfilter und neues kleines FilletToolController-Modul hinzufügen.
  2. Mehrfachkante, Radius, Previewfehler, OK/Cancel/Edit an bestehenden Host anbinden.
  3. Featureparameter in Tree/Properties und Store aufnehmen; Codecänderung nach D13-Vertrag.
- **Abnahme:** Fillet erzeugen, Radius ändern, Cancel, Undo/Redo, Save/Open über echte UI funktionieren. Ungültige Auswahl hält korrigierbares Panel offen. A-UI-02 auf Fillet angewendet.
- **Grenze:** Keine direkte Kerneloperation aus dem Widget.

## M07 – Einfache Chamfer-Funktion mit demselben Fachpfad

- **Voraussetzungen:** M06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ChamferFeature für symmetrischen Abstand an stabilen Kantenreferenzen definieren.
  2. Kerneloperation plus History und kleiner Controller nach bewährtem Fillet-Vertrag implementieren.
  3. Nicht unterstützte Distanz/Winkel-Varianten im UI nicht anbieten; Persistenz und Edit unterstützen.
- **Abnahme:** Ein Boxrand erhält erwartete ebene Fase; negativer/zu großer Abstand rejected; Edit/Undo/Save/Open erhält Referenzen. A-FILLET-01 analoge Chamferfälle bestehen.
- **Grenze:** Asymmetrische/Winkelfase ist ein späterer eigener Auftrag.

## M08 – Parametrische einfache Bohrung liefern

- **Voraussetzungen:** M04, R06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/kernel/ShapeOperations.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. HoleSpec mit Position aus Skizzenpunkt oder explizitem Bezug, Achse, Durchmesser und Blind/ThroughAll festlegen.
  2. Bohrvolumen und Ziel-Cut aus gemeinsamen Kernelbausteinen erzeugen; Durchgangslänge aus Körperbezug bestimmen.
  3. Eigenen Controller mit Preview/Edit/Persistenz ergänzen; Referenzverlust sichtbar behandeln.
- **Abnahme:** Zylinderbohrung durch Quader hat analytisch korrektes Volumen; Blindtiefe endet richtig, ThroughAll bleibt nach Körperdickenänderung durchgehend. A-HOLE-01 besteht.
- **Grenze:** Gewinde, Senkung und Normteilkatalog erst nach eigenem Fachvertrag; keine bloße kosmetische Gewindebehauptung.

## M09 – Parametrische Körperspiegelung

- **Voraussetzungen:** M01, M03, R03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/kernel/ShapeOperations.cpp`, `include/voelund/document/ModelDocument.hpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. MirrorFeature mit Eingabekörper, stabiler Spiegelbezugsebene und NewBody/Join-Modus ergänzen.
  2. Orientierung/Handedness korrigiert an Kernel übergeben; Instanzherkunft in SubshapeRefs erhalten.
  3. Kleinen Controller, Persistenz und Edit über vorhandene Registry-/Toolmuster anschließen.
- **Abnahme:** Asymmetrisches Referenzteil wird geometrisch gespiegelt, nicht nur verschoben. Mastermaßänderung aktualisiert Ergebnis; Join-Fehler ist lokalisiert. Undo/Save/Open stabil.
- **Grenze:** Featuremuster versus Körpermuster nicht vermischen; v1 startet ausdrücklich mit Körpermuster.

## M10 – Lineare und kreisförmige Körpermuster

- **Voraussetzungen:** M09, R01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/kernel/ShapeOperations.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. PatternSpec mit stabiler Quelle, Instanzindex, Count, Spacing bzw. Axis/Angle definieren; Kreis-Endpunktdopplung festlegen.
  2. Count-/Ressourcengrenzen, Preview und parametrische Neubewertung implementieren.
  3. Linear und Circular nacheinander als M10a/b nach identischem Vertrag liefern; gemeinsame Instanzherkunft verwenden.
- **Abnahme:** 3× lineares Muster mit Abstand20 und 4× Kreis über360° erzeugt richtige Anzahl/Position ohne doppelte erste Instanz. Count0/zu groß rejected. Masteredit, Undo und Save/Open erhalten Instanzreferenzen.
- **Grenze:** Kein allgemeines parametrisches Feature-Replay-Muster ohne separate Input-Neuzuordnung.

## M11 – Platzierung und Transformation aller v1-Körper konsistent machen

- **Voraussetzungen:** M09, M10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/FeatureGraph.cpp`, `src/viewport/TransformManipulator.cpp`, `src/application/CadSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Bodyplacement von ursprünglichem Primitive-Featuretyp lösen; Extrusionskörper ebenfalls transformierbar machen.
  2. Lokale versus Weltkoordinaten für Supportskizzen, Booleans und Muster eindeutig umrechnen.
  3. Manipulator führt temporäre Updates und einen Commit pro Drag aus; numerische Transformation nutzt denselben Befehl.
- **Abnahme:** Extrudiertes und primitives Teil lassen sich identisch verschieben/rotieren; aufgesetzte Skizze bleibt korrekt. Undo eines Drags stellt gesamten Zustand her. A-BODY-02 besteht.
- **Grenze:** Keine Einzelkopie der Weltgeometrie als Ersatz für parametrische Platzierung.

## M12 – Verlässliche Messung und Einheitenanzeige

- **Voraussetzungen:** M11, D10, U10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/MeasurementInput.cpp`, `src/viewport/CadViewportQtOpenGl.cpp`, `src/application/CadSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. MeasurementResult für Länge, Abstand, Winkel, Fläche und Volumen aus validem aktuellem Auswertungsstand anbieten.
  2. Pickingreferenzen und Einheiten/Genauigkeit eindeutig anzeigen; Displayrundung vom Wert trennen.
  3. Invalides/stale Ergebnis sperren oder ausdrücklich als historische Messung kennzeichnen.
- **Abnahme:** A-MEASURE-01 vergleicht Box, Zylinder, Ring und bekannte Winkel mit analytischen Werten. Kamera/DPI verändern Messung nicht. Auswahl-/Messvorgang macht Dokument nicht dirty.
- **Grenze:** Keine Toleranzprüfung/Zeichnungsbemaßung als vermeintlich identische Funktion.

## M13 – Historische Features bearbeiten

- **Voraussetzungen:** M06, M07, M08, M11, R01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. EditFeature öffnet Originalparameter und gültigen Inputzustand des historischen Features, nicht blind den aktuellen Bodytip.
  2. Preview berechnet Feature und abhängige Nachfolger als Kandidaten; OK übernimmt eine Transaktion.
  3. Fehler an Nachfolgern mit explizitem CommittedWithErrors anbieten und reparierbaren Zustand speichern.
- **Abnahme:** Frühe Skizzenbreite an Teil mit Pocket/Fillet ändern aktualisiert Kette; unabhängiger Körper unverändert. Cancel stellt gesamte Kette wieder her. A-HISTORY-01 besteht.
- **Grenze:** Nicht die gesamte spätere Timeline heimlich löschen, um Edit einfach zu machen.

## M14 – Suppress und Rollback als unterschiedliche Zustände implementieren

- **Voraussetzungen:** M13.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/features/FeatureGraph.cpp`, `src/application/CadSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Suppression persistiert fachlichen Featurezustand mit definierter Inputweitergabe bzw. blockierten Nachfolgern.
  2. Rollback-Cursor definiert Bearbeitungs-/Auswertungsgrenze; Verhalten von Einfügen und späteren Features explizit festlegen.
  3. Beide Benutzerbefehle getrennt implementieren und Undo-/Dirty-Semantik dokumentieren; bei Umfang M14a/b verwenden.
- **Abnahme:** Suppress→Save/Open→Unsuppress und Rollback→Edit→Rollforward erhalten IDs und Parameter. Abhängige Referenzen werden nicht neu geraten. A-HISTORY-02 besteht.
- **Grenze:** Rollback ist kein Löschen und Suppression keine bloße Sichtbarkeit.

## M15 – Echte Feature-Timeline mit Auswahl, Edit und Fehlern

- **Voraussetzungen:** M14, U06.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/ModelTreePanel.cpp`, `src/desktop/MainWindow.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Timeline aus echter Featureprojektion rendern; Auswahl zwischen Timeline/Tree/Viewport synchronisieren.
  2. Edit/Suppress/Rollback-Befehle über Registry anbinden; Fehler/Blocked/Suppressed und aktuellen Cursor zeigen.
  3. Tastaturbedienung und lange Listen ohne starre Featureanzahl unterstützen.
- **Abnahme:** Timeline des gespeicherten Referenzteils entspricht tatsächlichen IDs/Reihenfolge. Rechtsklick/Edit und Undo funktionieren; keine festen Sketch/Pad/Fillet-Labels bleiben. A-HISTORY-01/02 über UI bestehen.
- **Grenze:** Kein erneuter eigener Auswertungszustand in Timelinewidgets.

Zusätzlicher Pflichtnachweis: A-ERR-01-Timeline-Stufe aus Kapitel 08; ein D07-
Fundamentnachweis ersetzt die Prüfung der echten Timeline nicht.

## M16 – Reorder und Delete mit Abhängigkeitsprüfung

- **Voraussetzungen:** M15, R01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSession.cpp`, `src/features/FeatureGraph.cpp`, `src/desktop/ModelTreePanel.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Reorder nur zulassen, wenn alle benötigten Inputs verfügbar bleiben; ungültige Verschiebung vor Mutation erklären.
  2. Delete mit konkreter Liste betroffener abhängiger Features anbieten; einzelne Referenzreparatur von bewusstem Kaskadenlöschen unterscheiden.
  3. Reorder/Delete als eine Transaktion implementieren, IDs unabhängiger Objekte erhalten.
- **Abnahme:** Gültige unabhängige Umordnung funktioniert; Feature vor seinen Input ziehen wird abgewiesen. Delete mit Abhängigkeiten verliert nichts ohne die gewählte Aktion. Undo stellt Graph und Auswahl wieder her.
- **Grenze:** Keine generische 'alle späteren Features löschen'-Heuristik.

## M17 – STEP-Import/Export am Kernel-/Dokumentrand

- **Voraussetzungen:** M12, D13, R03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/kernel/CMakeLists.txt`, `src/kernel/ShapeOperations.cpp`, `tools/build-freebsd-occt.sh`, `src/document/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Benötigte OCCT-DataExchange-Toolkits exakt hinzufügen; Importfeature mit eingebettetem Quell-BRep und Quelleinheiten definieren.
  2. STEP lesen/schreiben mit Units, mehreren Bodies und vereinbarter Namensübernahme; keine externe Quelldatei als einzige Dokumentwahrheit.
  3. Import-/Exportfehler, Teilresultate und inkompatible Shapes in typed Result abbilden; exportiert wird nur explizit gültige Auswahl/Revision.
- **Abnahme:** A-STEP-01: eigenes Mehrkörperteil exportieren/importieren, Solidanzahl, Boundingbox und Volumen innerhalb Toleranz vergleichen. Inches-Fixture korrekt in mm; kaputte STEP-Datei liefert Fehler ohne aktives Dokument zu ersetzen.
- **Grenze:** Kein Versprechen einer parametrischen Timeline aus STEP; Assemblystruktur/XDE-Ausbau separat X01.

Zusätzlicher Pflichtnachweis: Exportversuch einer fehlerhaften aktuellen Revision
ablehnen, auch wenn eine letzte gültige Darstellung vorhanden ist (A-ERR-01).

## M18 – Austauschworkflow und mechanische v1-Abnahme

- **Voraussetzungen:** M16, M17.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/DocumentActions.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `tests/integration/CadSessionTests.cpp`, `docs/development-status.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Import als neue/gewählte Dokumentaktion und Export ausgewählter Körper mit sichtbaren Einheiten/Ergebnis anbinden.
  2. Fehler, Cancel und Save/Open eines importierten Features vollständig durchführen.
  3. Mechanische Referenzmodelle aus Kapitel08 über produktive Shell erstellen, parametrisch ändern und austauschen; G3-Bericht schreiben.
- **Abnahme:** A-PART-01/02, A-STEP-01, A-HISTORY-01/02, A-UI-01 komplett bestanden. Handbuch nennt tatsächlich umgesetzte Varianten und Grenzen. Keine aktive Attrappe im v1-Umfang.
- **Zusätzliche Gesamtprüfung:** A-ERR-01 über gemeinsame Shell, echte Timeline,
  Reparatur/Undo und produktiven Exportpfad vollständig wiederholen. Die vorherigen
  Teilnachweise allein schließen die Produktabnahme nicht ab.
- **Grenze:** G3 ist noch keine Plattform-/Releasefreigabe; L-Pakete bleiben Pflicht.

## Ergänzung vom 14.10.2026: fehlende Werkzeuge

Abgleich mit den Werkzeugleisten von FreeCAD 1.1 (Sketcher und Part Design). Nutzerentscheidung vom 14.10.2026: Gruppe 1 gehört zu v1, Gruppe 2 wird aus X19 vorgezogen; beide werden vor M09 umgesetzt (Reihenfolge in `packets.json`, `topologicalOrder`).

## M19 – Bezugsebene und Bezugsachse

- **Voraussetzungen:** M04.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/features/FeatureGraph.cpp`, `src/document/NativePrimitiveCodec.cpp`, `src/desktop/MainWindowCommands.cpp`.
- **Auftrag:**
  1. Feature Bezugsebene: versetzt zu Basisebene oder ebener Körperfläche (Abstand), optional um eine Achse gedreht (Winkel); Feature Bezugsachse: Basisachse, gerade Körperkante oder Achse einer Zylinderfläche.
  2. Bezüge folgen ihrer Quelle über Namen; fehlende Quelle wird lokalisiert gemeldet. Bezüge erscheinen im Baum und in der Timeline.
  3. Skizze auf Bezugsebene; Bezugsachse ist Achse für Drehen (M20) und Wendel (M26).
- **Abnahme:** Ebene 25 mm über XY: Skizze darauf extrudiert beginnt bei z=25; Abstand ändern → Körper folgt; Ebene versetzt zu Quaderdeckfläche folgt der Quaderhöhe; Achse aus Zylindermantel; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Ebene durch drei Punkte oder tangential an Flächen in diesem Paket; kein Bezugskoordinatensystem. Nutzerentscheidung 2026-10-14: Gruppe 1, gehört zu v1 (G3).

## M20 – Drehen (Revolve) und Nut

- **Voraussetzungen:** M19, M04.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/features/ExtrusionEvaluation.cpp`, `src/kernel/ShapeOperations.cpp`, `src/desktop/SketchExtrusionToolController.cpp`.
- **Auftrag:**
  1. Feature Drehen: Skizzenbereiche, Achse (Skizzenlinie, Basisachse oder Bezugsachse), Winkel bis 360°, einseitig/umgekehrt/symmetrisch, Operation Neuer Körper/Verbinden/Schneiden/Schnitt mit Zielkörper.
  2. Kernel BRepPrimAPI_MakeRevol mit History; Start-, End- und Mantelflächen semantisch benennen, damit Skizzen, Verrundung und Bohrung darauf greifen.
  3. Ein Befehl solid.create.revolve mit Panel (wie Extrusion); Nut ist Drehen mit Schneiden, kein eigener Befehl. Achse, die das Profil schneidet, wird abgewiesen.
- **Abnahme:** Rechteck 10×20 im Abstand 5 von der Achse, 360° → Hohlzylinder mit Volumen π(15²−5²)·20; 90° → ein Viertel; Nut schneidet Ring aus Welle; Profilmaß ändern → Ergebnis folgt; Achse durch das Profil abgewiesen; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Endbedingung bis Fläche; kein Drehen von Körperflächen. Nutzerentscheidung 2026-10-14: Gruppe 1, gehört zu v1 (G3).

## M21 – Boolesche Operation zwischen Körpern

- **Voraussetzungen:** M03.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/features/FeatureGraph.cpp`, `src/kernel/ShapeOperations.cpp`, `src/desktop/MainWindowCommands.cpp`.
- **Auftrag:**
  1. Feature Kombinieren: Zielkörper, ein oder mehrere Werkzeugkörper, Operation Verbinden/Abziehen/Schnitt, Werkzeuge behalten ja/nein.
  2. Werkzeugkörper ohne Behalten verschwinden als eigenständige Körper, bleiben aber über die Historie editierbar; ihre Flächen behalten Namen im Ergebnis.
  3. Panel solid.modify.combine: Ziel und Werkzeuge im Viewport oder Baum wählen; leeres Ergebnis wird erklärt.
- **Abnahme:** Quader minus Zylinder ergibt das analytische Volumen; Werkzeug behalten lässt beide Körper sichtbar; Zylinderradius ändern → Ergebnis folgt; Schnitt ohne Überlappung abgewiesen; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Baugruppen- oder Komponentenlogik. Nutzerentscheidung 2026-10-14: Gruppe 1, gehört zu v1 (G3).

## M22 – Wandstärke (Schale)

- **Voraussetzungen:** M05, M21.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`, `src/desktop/MainWindowCommands.cpp`.
- **Auftrag:**
  1. Feature Wandstärke: Körper, zu öffnende Flächen (über Namen), Dicke, nach innen/außen.
  2. Kernel BRepOffsetAPI_MakeThickSolid mit History; Innenflächen benennen; zu große Dicke wird lokalisiert gemeldet.
  3. Panel solid.modify.shell: Flächen anklicken (mehrfach, auch über der Vorschau), Dicke ändern.
- **Abnahme:** Quader 40×30×20 mit offener Deckfläche, Dicke 2 → Volumen analytisch; zweite Fläche öffnen; Dicke 20 abgewiesen; Quadermaß ändern → Schale folgt; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine unterschiedlichen Dicken je Fläche. Nutzerentscheidung 2026-10-14: Gruppe 1, gehört zu v1 (G3).

## M23 – Folgen (Sweep) entlang eines Pfades

- **Voraussetzungen:** M20.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`.
- **Auftrag:**
  1. Feature Folgen: Profilbereiche einer Skizze, Pfad aus zusammenhängenden Elementen einer anderen Skizze, Operation wie Extrusion.
  2. Kernel BRepOffsetAPI_MakePipe(Shell) mit korrigierter Ausrichtung; Flächen benennen.
  3. Panel solid.create.sweep: Profil wählen, Pfad wählen.
- **Abnahme:** Kreis Ø6 entlang geradem Pfad ergibt analytisches Volumen, entlang L-Pfad mit Bogen gültig; Pfad ändern → Ergebnis folgt; Schneiden als Nut; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Führungskurven, keine Skalierung entlang des Pfads. Nutzerentscheidung 2026-10-14: Gruppe 2, aus X19 vorgezogen.

## M24 – Ausformung (Loft) zwischen Profilen

- **Voraussetzungen:** M19.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`.
- **Auftrag:**
  1. Feature Ausformung: zwei oder mehr Profile (je ein Bereich aus Skizzen auf verschiedenen Ebenen), glatt oder geradlinig, Operation wie Extrusion.
  2. Kernel BRepOffsetAPI_ThruSections; Anfang/Ende/Mantel benennen.
  3. Panel solid.create.loft: Profile in Reihenfolge wählen.
- **Abnahme:** Quadrat 20 auf XY zu Quadrat 10 auf Bezugsebene z=30 geradlinig ergibt Pyramidenstumpfvolumen; Profilmaß ändern → folgt; Profile in derselben Ebene abgewiesen; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Führungskurven, keine Punktprofile. Nutzerentscheidung 2026-10-14: Gruppe 2, aus X19 vorgezogen.

## M25 – Formschräge

- **Voraussetzungen:** M22.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`.
- **Auftrag:**
  1. Feature Formschräge: Flächen eines Körpers, neutrale ebene Fläche, Winkel.
  2. Kernel BRepOffsetAPI_DraftAngle mit History; nicht neigbare Flächen lokalisiert melden.
  3. Panel solid.modify.draft: Flächen anklicken, neutrale Fläche wählen, Winkel.
- **Abnahme:** Quaderseiten mit 5° auf Grundfläche geneigt: Volumen analytisch; Winkel ändern → folgt; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Trennlinien-Formschräge. Nutzerentscheidung 2026-10-14: Gruppe 2, aus X19 vorgezogen.

## M26 – Wendel (Helix)

- **Voraussetzungen:** M23.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeOperations.cpp`, `src/features/FeatureGraph.cpp`.
- **Auftrag:**
  1. Feature Wendel: Profilbereich, Achse (Skizzenlinie/Basis-/Bezugsachse), Steigung, Höhe, links/rechts, Operation wie Extrusion.
  2. Helixpfad auf Zylinderfläche, Sweep mit Hilfsfläche; Flächen benennen.
  3. Panel solid.create.helix.
- **Abnahme:** Kreisprofil Ø2 im Abstand 10, Steigung 5, Höhe 20 → Feder mit Volumen innerhalb 1 % des analytischen Werts; Schneiden als Gewindenut; Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine kegelige Wendel, kein Normgewinde. Nutzerentscheidung 2026-10-14: Gruppe 2, aus X19 vorgezogen.

## M27 – Weitere Grundkörper

- **Voraussetzungen:** M01.
- **Pflichtlektüre/Einstieg:** Kapitel 02 K01–K08; `include/voelund/document/ModelDocument.hpp`, `src/kernel/ShapeFactory.cpp`, `src/features/FeatureGraph.cpp`, `src/desktop/MainWindowCommands.cpp`.
- **Auftrag:**
  1. Kugel, Kegel(-stumpf), Torus und Keil als Grundkörper mit Platzierung und Ergebnis Neuer Körper/Verbinden/Schneiden/Schnitt wie der Zylinder.
  2. Flächen semantisch benennen; Panel je Grundkörper mit seinen Maßen.
- **Abnahme:** Volumen je Grundkörper analytisch; Kugel von Quader abgezogen; Maß ändern, Rückgängig, Speichern/Öffnen.
- **Grenze:** Keine Ellipsoide/Prismen mit freiem Querschnitt (dafür Skizze + Extrusion). Nutzerentscheidung 2026-10-14: Gruppe 2, aus X19 vorgezogen.
