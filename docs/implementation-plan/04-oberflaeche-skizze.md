# 04 – Eine Anwendungsschale und ein vollständiger Skizzenablauf

Grundlage der Oberfläche ist [ADR 0012](../adr/0012-ui-shell-concept.md) (freigegeben 2026-10-04); die Gestaltung der UI-Vorschau gilt nicht mehr. K03/K04/K07 sind Pflichtverträge. Dieses Kapitel baut kein zusätzliches Frontend, sondern führt vorhandene Funktionen und Darstellung zusammen. U11 wartet ausdrücklich auf die benötigten S-Pakete; die IDs sind keine starre numerische Reihenfolge.

**Prüfweg:** vorhandene `application.sketch_*`-Tests und `workspaces.module_registry`, neue fachliche Regressionen und ab U11 echte UI-Ereignisse. Bei Darstellung mit gleicher Fenstergröße/DPI visuell vergleichen; Bilder ersetzen keine Prüfung des tatsächlichen Dokumentzustands.

## U01 – Sichtbare Befehle mit tatsächlicher Funktion abgleichen

- **Voraussetzungen:** A08, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/desktop/MainWindow.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/workspaces/CommandRegistry.cpp`, `docs/ui-concept.md`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Befehlstabelle des Produkts mit ID, Einstieg (Werkzeugleiste, Kontextleiste, Skizzenleiste, Befehlssuche, Kürzel), Controller, Aktivierungsbedingung und Abnahmetest anlegen.
  2. Doppelte und funktionslose Einstiege (z. B. „Sketch Rectangle“ neben „Sketch“, Connection auf den letzten Zylinder) entfernen oder eindeutig sperren; Aktivierung für Ebene, Fläche, Skizzenbereich und Körper festlegen.
  3. Start mit leerem Dokument statt Box/Zylinder; feste Platzhalter durch tatsächlichen Zustand ersetzen.
- **Abnahme:** Kein aktiver Knopf ohne registrierte ausführbare Aktion; jeder Befehl der Tabelle ist über genau eine Command-ID erreichbar. Nicht verfügbares Werkzeug verändert weder Auswahl noch Historie.
- **Grenze:** Noch keine neue Gestaltung (U03); bereinigt die Befehlsbasis für ADR 0012. UI-Vorschau wird nicht mehr angepasst.

## U02 – Gemeinsames Shell-ViewModel definieren

- **Voraussetzungen:** U01, D04, UX01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `include/voelund/desktop/MainWindow.hpp`, `src/desktop/MainWindow.cpp`, `src/desktop/`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Verantwortlichkeiten der Produktshell nach ADR 0012 zuordnen: Kopfzeile, Modellbaum, Werkzeugleiste, Kontextleiste, Werkzeugpanel, Timeline, Parameterpanel, Viewport.
  2. Kleine read-only Projektion für Dokumenttitel, Speicherzustand, aktuelle Auswahl, Befehlsverfügbarkeit und Werkzeugzustand bereitstellen.
  3. Ownership und Lebensdauer von CadSession, Viewport und Panels im Composition Root dokumentieren.
- **Abnahme:** Komponentenskizze benennt für jeden Zustand genau einen Besitzer. Keine zweite CadSession pro Ansicht und keine direkten Featuremutatoren aus Widgets.
- **Grenze:** API-/Strukturpaket; keine gleichzeitige Migration aller UI-Komponenten. Grundlage ADR 0012.

## U03 – Gestaltung nach ADR 0012 als Desktop-Komponenten umsetzen

- **Voraussetzungen:** U02, A03, UX01.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/desktop/`, `src/viewport/`, `resources/`, `src/desktop/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Farbtoken für hell und dunkel, Schriften (Albert Sans, JetBrains Mono, eingebettet) und ein einheitliches Symbolset als Desktop-Komponenten anlegen; Umschalter hell/dunkel.
  2. Grundlayout der Shell: Kopfzeile, Baum links, Viewport mit ViewCube und Standardansichten, Fläche für Timeline; Viewportfarben aus den Token.
  3. Oberflächentexte deutsch über tr(); Aufnahmen hell/dunkel bei gleicher Größe als Vergleichsbasis.
- **Abnahme:** Produkt startet im Layout des freigegebenen Prototyps in heller und dunkler Darstellung; Komponenten hängen nicht von voelund-ui-preview ab. Start-Smokes grün; Aufnahmen selbst geprüft und dem Nutzer vorgelegt.
- **Grenze:** Nur Darstellung; keine Fachlogik und kein Parametermodell in Style-/Symbolmodulen. Grundlage ist ADR 0012, nicht mehr die Gestaltung der UI-Vorschau.

## U04 – Werkzeugleiste, Kontextleiste und Befehlssuche an CommandRegistry anbinden

- **Voraussetzungen:** U02, U03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/workspaces/CommandRegistry.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/desktop/WorkspaceToolbar.cpp`, `src/desktop/`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Senkrechte, ausblendbare Werkzeugleiste aus registrierten Befehlen und Modulmetadaten erzeugen; unpassende Werkzeuge gesperrt statt versteckt.
  2. Schwebende Kontextleiste zeigt je Auswahl (nichts, Ebene, Fläche, Skizzenbereich, Körper, Skizzenmodus) die passenden Befehle; der nächste Schritt ist hervorgehoben.
  3. Befehlssuche (Strg+K), Tastenkürzel und Kontextmenü verwenden dieselbe Command-ID und Zustandsprüfung; Aktivierung bei Auswahl-/Werkzeug-/Dokumentwechsel aktualisieren.
- **Abnahme:** Werkzeugleiste, Kontextleiste, Befehlssuche und Kürzel lösen für dieselbe ID denselben Application-Befehl aus. Unbekannte ID liefert Diagnose. Kein Befehl ist nur über die Kontextleiste erreichbar.
- **Grenze:** Noch nicht implementierte Fachtools bleiben gesperrt. Kein Ribbon.

## U05 – Schwebendes Werkzeugpanel über den generischen ToolPanelHost

- **Voraussetzungen:** U04, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/desktop/ToolPanelHost.cpp`, `src/desktop/SketchEditorToolController.cpp`, `src/desktop/SketchExtrusionToolController.cpp`, `src/desktop/`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ToolPanelHost besitzt genau eine aktive Sitzung und zeigt sie als schwebendes Panel am Viewport; Parameterfelder sind reine Bindung an deren State.
  2. OK/Abbrechen/Escape/Fokuswechsel gemäß K04 vereinheitlichen; Toolwechsel bei ungültiger Skizze blockiert mit Fehlermeldung.
  3. Docks „Properties“/„Tool“ ablösen; vorhandene Controller über den Application-Vertrag anschließen; erweiterte Optionen einklappbar, nie weggelassen.
- **Abnahme:** A-UI-02 besteht für OK, Abbrechen, Escape und Toolwechsel. Abbrechen lässt Dokument/Undo/Dirty unverändert. Fehlgeschlagene Skizze startet kein Extrusionspanel.
- **Grenze:** Keine zweite temporäre Parametersammlung. Layout nach ADR 0012.

## U06 – Modellbaum mit Ursprüngen, Sichtbarkeit, Raster und Timeline aus echter Dokumentprojektion

- **Voraussetzungen:** U02, U05, D11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/desktop/ModelTreePanel.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`, `src/desktop/MainWindow.cpp`, `src/viewport/BasePlanePresentation.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Baum liest echte Körper-/Feature-IDs, Namen, Fehler und Unterdrückung; Ursprung des Dokuments und je Körper mit XY/XZ/YZ; Augen-Schalter für Ebenen, Skizzen, Körper und das Raster auf XY. Sichtbarkeit ist Ansichtszustand ohne Undo/Dirty.
  2. Basisebenen aus V01b an die Ursprünge binden: im leeren Dokument sichtbar, neben Körpern dezent, Klicks auf Körper haben Vorrang.
  3. Timeline unten als Projektion der Featurereihenfolge; Klick öffnet das Feature über die vorhandenen Edit-Wege. Titel aus Dokumentname und Speicherzustand; Auswahl zwischen Baum, Viewport und Timeline über typed SelectionRef synchronisieren.
- **Abnahme:** Neues/geladenes Dokument zeigt korrekte Inhalte. Ausblenden einer Ebene im Baum entfernt sie aus Viewport und Pick, ohne Dirty. Umbenennen und Undo aktualisieren alle Ansichten. Timeline-Klick öffnet Skizze bzw. Extrusion.
- **Grenze:** Rückrollmarke, Umordnen und Fehleranzeige in der Timeline folgen M15; hier nur Projektion und Öffnen.

## U13 – Werkzeugleiste passt sich dem aktiven Modus an (Skizzenmodus)

- **Voraussetzungen:** U05, U04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/desktop/ToolRail.cpp`, `src/desktop/SketchEditorToolController.cpp`, `src/desktop/ToolPanelHost.cpp`, `src/desktop/ContextCommands.cpp`, `tests/desktop/`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Werkzeugleiste zeigt je Modus deren Werkzeuge: außerhalb einer Skizze die Volumenwerkzeuge, im Skizzenmodus Zeichenelemente, Beziehungen, Bemaßungen und Bearbeitung (Stutzen, Versetzen, Muster usw.), gruppiert und mit Symbolen.
  2. Skizzenwerkzeuge als registrierte Befehle mit Command-ID, sodass Werkzeugleiste, Befehlssuche und Kürzel denselben Weg nutzen; das Werkzeugpanel behält nur Werte, Rückmeldung und OK/Abbrechen.
  3. Gesperrt statt versteckt nach Auswahl im Skizzenmodus; Wechsel des Modus aktualisiert die Leiste ohne Dokumentänderung.
- **Abnahme:** Im Skizzenmodus zeigt die Werkzeugleiste alle Skizzenwerkzeuge und keine Volumenwerkzeuge; nach OK/Abbrechen wieder die Volumenwerkzeuge. Jedes Skizzenwerkzeug ist über Werkzeugleiste, Befehlssuche und Kürzel mit derselben Command-ID erreichbar. Bestehende Skizzentests grün; Aufnahmen gesichtet und vorgelegt.
- **Grenze:** Nutzerentscheidung vom 2026-10-04 (Ergänzung zu ADR 0012). Keine neuen Skizzenfunktionen; nur deren Einstieg. Fachregeln bleiben in den Controllern.

## U07 – New/Open/Save und Close-Schutz in der gemeinsamen Shell anschließen

- **Voraussetzungen:** U05, D03, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/DocumentActions.cpp`, `src/desktop/MainWindow.cpp`, `apps/voelund-cad/main.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Dokumentaktionen mit dem sicheren Store verbinden und Fehler verständlich anzeigen.
  2. Dirty-New/Open/Close bietet Save/Discard/Cancel; fehlgeschlagener Save führt nicht trotzdem zum Schließen.
  3. Native Pfadkonvertierung für Windows und Unix eindeutig gestalten; Unicode-/Leerzeichenfälle verwenden.
- **Abnahme:** A-IO-01 und A-UI-03 bestehen: Cancel erhält Modell; Savefehler hält Fenster offen; Openfehler zerstört kein aktives Dokument; Save-As setzt Titel erst nach Erfolg.
- **Grenze:** Keine neue cloudbasierte Dokumentverwaltung.

## U08 – Skizzen-Erzeugung auf einen Weg vereinheitlichen

- **Voraussetzungen:** U05, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `apps/voelund-ui-preview/SketchBackend.cpp`, `src/desktop/SketchEditorCreation.cpp`, `src/application/SketchEditorSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Gemeinsamen Application-Erzeugungsbefehl mit semantischen Ergebnisrollen vorbereiten; der Produktweg ist maßgeblich.
  2. Duplizierte Snap-/Constraint-Erzeugung aus UI entfernen; Renderingadapter erhalten nur Preview-/Result-Daten.
  3. Robuste Legacy-Fälle als Charakterisierungstests erhalten; der zweite Weg der UI-Vorschau wird nicht weiterentwickelt.
- **Abnahme:** Rechteck, Circle, Arc und Ellipse aus beiden bisherigen Einstiegen erzeugen fachlich gleichen Editorzustand. Kein UI-Code berechnet IDs durch Einfügereihenfolge.
- **Grenze:** Die UI-Vorschau mit ihrem SketchBackend ist mit U12a entfernt; maßgeblich ist der Produktweg.

## U09 – Input und Drag als einzelne Benutzeraktion behandeln

- **Voraussetzungen:** U05, D06, U08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/viewport/SketchEditorInteraction.cpp`, `src/desktop/SketchEditorPresentation.cpp`, `src/application/SketchEditorHistory.cpp`, `apps/voelund-ui-preview/SketchBackend.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Begin/Update/End/Cancel für Drag definieren; Update verändert nur temporären Kandidaten.
  2. History-Aufzeichnung aus Renderrefresh entfernen; Pointer-up erzeugt höchstens einen Schritt, abgebrochener Drag keinen.
  3. Keyboard-Nudge, numerische Eingabe und Mehrfachselektion mit klaren separaten Grenzen anbinden.
- **Abnahme:** A-TXN-03: 100 MouseMoves → ein Undo; Escape während Drag → Ausgangszustand; bloßes Refresh → null Schritte. Wiederholtes Redraw verändert Dirty nicht.
- **Grenze:** Keine Zeitdebounce-Heuristik als Ersatz für echte Eingabegrenzen.

## U10 – Fehler und reparierbare Referenzen sichtbar machen

- **Voraussetzungen:** U06, R06, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/ModelTreePanel.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`, `src/desktop/ToolPanelHost.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Featurefehler mit betroffenen IDs und Handlungsoption Edit/Repair anzeigen.
  2. Stale-Geometrie klar kennzeichnen und Export/Messung des ungültigen aktuellen Ergebnisses sperren.
  3. Explizites Übernehmen mit Folgefehlern aus K03 und Referenzneuzuweisung integrieren.
- **Abnahme:** A-ERR-01 und A-REF-03 im UI bedienbar; Fehler verschwindet nach echter Reparatur, nicht nach Refresh. Undo der Reparatur stellt Fehlzustand wieder her.
- **Grenze:** Keine generischen zusätzlichen Bestätigungsdialoge bei jedem gültigen Edit.

## U11 – Vollständigen UI-Fluss automatisch prüfen

- **Voraussetzungen:** U07, U08, U09, U10, S03, S07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `tests/CMakeLists.txt`, `apps/voelund-cad/main.cpp`, `src/desktop/MainWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Qt-Test klickt echte registrierte Aktionen in Werkzeugleiste, Kontextleiste und Panel, setzt Felder und bestätigt nach Eventloopstart.
  2. Ursprung/Ebene→Skizze→Constraint→Skizze fertig→Bearbeiten über Timeline→Abbrechen→Speichern/Öffnen über die tatsächliche UI ausführen.
  3. Rendering-Readiness, Dokumentrevision, Baum-/Timeline-Inhalte und Fehlerzustände prüfen; Aufnahmen hell/dunkel ergänzen fachliche Assertions.
- **Abnahme:** A-UI-01 Skizzenteil und A-UI-02/03 bestehen. Absichtlich abgetrennter Handler macht Test rot. Extrude-Teil wird in M04 ergänzt. A-VIS-01 bestanden: Bilder tatsächlich selbst betrachtet, Layout-/Overlayfehler korrigiert und mit neuer Aufnahme nachgeprüft.
- **Grenze:** Kein Test, der nur Backendmethoden aufruft und danach Fenstersichtbarkeit bestätigt.

## U12a – Altlasten der Oberfläche vor dem Weiterbau entfernen

- **Voraussetzungen:** U06, U13. Nutzerentscheidung vom 04.10.2026, vorgezogener Teil von U12.
- **Auftrag:**
  1. voelund-ui-preview mit Ribbon, Fake-Canvas, Fake-Timeline, eigenem SketchBackend und Starttests entfernen; der Plattformbericht prüft nur noch das Produkt.
  2. Toten Rechteckweg im Viewport (`SketchInteraction`, `beginRectangleSketch`, Rechteck- und Extrusionsvorschau-Handler) entfernen; Flächenauswahl-Hilfen bleiben.
  3. Desktop-Tests schreiben Einstellungen nur in ein temporäres INI-Verzeichnis je Prozess.
- **Abnahme:** Build und alle Tests grün unter Windows und in der Linux-CI ohne voelund-ui-preview. Kein Aufrufer des alten Rechteckwegs. Kein Desktop-Test liest oder schreibt dauerhafte Einstellungen.
- **Grenze:** Keine sichtbare Änderung am Produkt. Testgerüste mit einzelnen Panels bleiben; Composition Root und Installation bleiben U12.

## U12 – Eine produktive Anwendung und einen Composition Root festlegen

- **Voraussetzungen:** U03, U04, U05, U06, U07, U11, U12a.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `apps/voelund-cad/CMakeLists.txt`, `src/desktop/MainWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Produkt `voelund-cad` mit der Shell nach ADR 0012 bauen und installieren.
  2. Vorschau und toter Rechteckweg sind mit U12a entfernt; verbleibende verwaiste Handler mit entfernen.
  3. Screenshots und Doku auf den produktiven Einstieg umstellen.
- **Abnahme:** Installiertes Produkt startet dieselbe getestete Shell. Kein zweiter SketchBackend mit eigenen Regeln, keine Fake-Timeline. Ein kompletter New/Sketch/Save/Open-Fluss besteht.
- **Grenze:** Die Vorschau ist mit U12a entfernt (Nutzerentscheidung 04.10.2026); ihre Gestaltung war seit ADR 0012 keine Vergleichsbasis mehr.

## S01 – Semantischen SketchCreationResult abschließen

- **Voraussetzungen:** U08, R10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/application/SketchEditorSession.hpp`, `src/application/SketchEditorGeometry.cpp`, `src/application/SketchEditorRectangle.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Für Line, Polyline, Rectangle, Circle, Arc, Ellipse, Polygon und Slot Rollen und erzeugte Constraint-IDs definieren.
  2. Arc-Richtung, Kappenmittelpunkte und Ellipsenmittelpunkt unabhängig von Speicherreihenfolge bereitstellen.
  3. Auto-Constraint-Anwendung nur gegen vorhandene typisierte Rollen zulassen.
- **Abnahme:** Jede Werkzeugfamilie liefert eindeutige Rollen; umgekehrter Arc tauscht die richtigen Endpunkte, Slot-Snaps verwenden Kappenmitten. Unzulässige Rolle liefert Fehler statt ungültigem Constraint.
- **Grenze:** Keine UI-spezifischen IDs im Resultat.

## S02 – Editorzustand bei Solve/Accept/Cancel vollständig sichern

- **Voraussetzungen:** U09, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorSession.cpp`, `src/application/SketchEditorHistory.cpp`, `tests/integration/SketchEditorSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Editor-Snapshot enthält Elemente, Constraints, Auswahl, temporäre Referenzen, Solverstatus und History-Cursor.
  2. Reopen löst sofort und publiziert korrekte DOF statt -1; canAccept prüft zulässige Zustände explizit.
  3. Fehler nach Mutation und Validate werfen keinen halbgeänderten Zustand nach außen; Reject stellt Snapshot wieder her.
- **Abnahme:** Failed/Conflicting können nicht als gültig bestätigt werden. Unterbestimmte gültige Skizze kann entsprechend Vertrag akzeptiert werden. Cancel/Reopen/Undo stellen Auswahl und Solve-Status konsistent her.
- **Grenze:** Keine automatische Vollbestimmung als versteckte Voraussetzung offener Skizzen.

## S03 – Skizze aus leerem Dokument auf Basisebene erstellen

- **Voraussetzungen:** U05, S02, D11, R04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/CadSessionSketchProfiles.cpp`, `src/desktop/SketchEditorToolController.cpp`, `src/viewport/SketchViewOrientation.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. XY/XZ/YZ als explizite datumartige Ebenen mit stabiler Identität anbieten.
  2. Skizzenstart von Ebene oder gültiger planarer Stützfläche unterscheiden; vorhandenen Körper nicht voraussetzen.
  3. Orientierung, Rückkehrkamera und Editieren vorhandener Skizze über denselben Controller führen.
- **Abnahme:** Leeres Dokument→XY→Rechteck→OK funktioniert ohne Boxtrick. XZ/YZ und rotierte Stützfläche zeigen richtige Achsen; Cancel erzeugt kein Feature. A-UI-01 Skizzengrundlage besteht.
- **Grenze:** Keine allgemeine gekrümmte Supportfläche.

## S04a – Sichtbare Fangmarker, Beziehungsvorschau und Rechteck aus dem Mittelpunkt

- **Voraussetzungen:** V01, U13.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/viewport/SketchEditorInteraction.cpp`, `src/viewport/SketchPresentation.cpp`, `src/desktop/SketchEditorToolController.cpp`, `src/desktop/SketchToolActions.cpp`, `tests/desktop/`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Fangpunkte (Endpunkte, Mittelpunkte, Kreismittelpunkte, Ursprung, projizierte Punkte) im Skizzenmodus beim Hovern als Marker zeigen; der aktive Fangpunkt leuchtet in der Akzentfarbe.
  2. Die Beziehung, die beim Klick automatisch entsteht (deckungsgleich, waagrecht, senkrecht, auf Objekt, tangential, konzentrisch), als Symbol am Cursor vorab zeigen; Strg unterdrückt das Fangen.
  3. Rechteck aus dem Mittelpunkt als eigenes Zeichenwerkzeug (sketch.tool.center_rectangle) mit Kürzel; der Mittelpunkt bindet sich per Beziehung an einen gefangenen Punkt, z. B. den Ursprung.
- **Abnahme:** Qt-Test mit echten Ereignissen: Hover nahe dem Ursprung zeigt den aktiven Marker; ein Rechteck aus dem Mittelpunkt am Ursprung bleibt nach einer Maßänderung zentriert; die angekündigte Beziehung entsteht tatsächlich. Aufnahmen gesichtet und dem Nutzer vorgelegt.
- **Grenze:** Nutzerwunsch vom 2026-10-04 (nach U13). Nur Sichtbarkeit und Mittelpunkt-Rechteck; fachliche Korrekturen von Fangen und Auto-Constraints bleiben S04/S05.

## S04 – Snap und Auto-Constraints für lineare Werkzeuge korrigieren

- **Voraussetzungen:** S01, S02, S03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorPointConstraints.cpp`, `src/desktop/SketchEditorCreation.cpp`, `tests/integration/SketchAlignmentTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Line/Polyline/Rectangle an Endpoint, Origin, horizontal/vertikal und projizierte Geometrie anbinden.
  2. Beide Rechteckecken und Folgepunkte korrekt referenzieren; doppelte redundante Constraints vermeiden.
  3. Auto-Constraints-Checkbox tatsächlich schalten und erzeugte Beziehungen verständlich anzeigen.
- **Abnahme:** Jeder Snaptyp an erster und zweiter Ecke geprüft; Checkbox aus erzeugt keine automatische Beziehung. Entfernte Projektion wird Referenzfehler statt falscher neuer Bindung.
- **Grenze:** Keine UI-Vorschau, die andere Constraints verspricht als anschließend gespeichert werden.

## S05 – Snap und Auto-Constraints für Kurven vervollständigen

- **Voraussetzungen:** S04, R05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorGeometry.cpp`, `src/application/SketchEditorRelations.cpp`, `tests/integration/SketchCircleConstraintTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Circle/Arc/Ellipse an richtige Mittelpunkt-/Endpunktrollen binden; 3-Punkt-Arc-Richtung berücksichtigen.
  2. Radial-/Tangential-/Konzentrisch-Beziehungen nur für fachlich unterstützte Kombinationen erzeugen.
  3. Numerische Singularitäten wie identische Arc-Punkte ohne Dokumentänderung abweisen.
- **Abnahme:** Circle an Linie tangential, Arc radial/tangential und Ellipsenmittelpunkt-Snap funktionieren in echter Erzeugung. Vertauschte Arc-Richtung bleibt korrekt; degenerierte Kurve liefert genaue Meldung.
- **Grenze:** Keine behauptete Tangentialität für vom Solveradapter nicht unterstützte Kombinationen.

## S06 – Polygon und Slot mit verbindlicher Semantik liefern

- **Voraussetzungen:** S01, S05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorConstruction.cpp`, `tests/integration/SketchConstructionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Regular-Polygon-Vertrag festlegen: Seitenanzahl, Mittelpunkt, Radiusart, Orientierung sowie tatsächlich erhaltene Winkel-/Längenbeziehungen.
  2. Slot mit Kappenmittelpunkten, Breite und Länge sowie horizontal/vertikal/frei definieren; erzeugte Kurvenrollen und Tangentialität prüfen.
  3. Wenn nur einmalige Kopiergeometrie unterstützt wird, dies explizit als solche anbieten; parametrisches Werkzeug nicht vorspiegeln.
- **Abnahme:** Maßänderung erhält Regularität bzw. Slot-Tangentialität. Freie Orientierung und vertauschte Eingabepunkte geprüft. Unsinnige Seitenzahl/negative Breite erzeugen keinen Teilzustand.
- **Grenze:** Nicht aus gleichen Polygonseiten allein auf Regularität schließen.

## S07 – Constraint-Labels, DOF und Dimensionsbindung vereinheitlichen

- **Voraussetzungen:** S02, S04, S05.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/features/SketchConstraintAnnotations.cpp`, `src/desktop/SketchConstraintLabel.cpp`, `src/desktop/SketchEditorConstraintState.cpp`, `src/viewport/SketchConstraintPresentation.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Eine Annotationprojektion für alle vorhandenen Constrainttypen liefern; Radius/Durchmesser/Winkel/Abstände korrekt unterscheiden.
  2. Labels und sichtbare Geometrie aus demselben Solve-Ergebnis beziehen; vollständig bestimmt, unterbestimmt, Konflikt und Failed korrekt anzeigen.
  3. Dimensionsbearbeitung als Application-Befehl mit Validierung und einem Undo umsetzen.
- **Abnahme:** Matrix aller im aktuellen Schema vorhandenen Constrainttypen besitzt erwarteten Labeltyp/Wert. DOF=0 zeigt vollständig bestimmt; Failed nicht under-constrained. Ungültiges Maß bleibt im Editor korrigierbar.
- **Grenze:** Keine neuen Constrainttypen ohne vollständigen Adapter-/Serializer-/UI-Pfad.

## S08 – Transformationen und Mehrfachauswahl korrigieren

- **Voraussetzungen:** S01, S02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorOperations.cpp`, `src/application/SketchEditorGeometry.cpp`, `tests/integration/SketchBasicToolsTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Mirror-Achsenwert vor Vektormutation sicher kopieren oder stabil über ID auflösen; keine dangling Pointer.
  2. Translate/Rotate/Mirror für jede unterstützte Geometrievariante und deren ausgewählte Rollen definieren.
  3. Legacy-Rechtecke bei nichtdarstellbarer Rotation kontrolliert in Linien migrieren; Delete verarbeitet gesamte vereinbarte Auswahl.
- **Abnahme:** Mehrere Spiegelkopien mit erzwungener Reallokation laufen unter verfügbarem Sanitizer. Ellipsenmittelpunkt lässt sich verschieben. Rotiertes Rechteck bleibt Rechteck; Undo stellt ursprüngliche IDs/Beziehungen nach Vertrag her.
- **Grenze:** Keine still ignorierten Geometrietypen bei erfolgreicher Rückgabe.

## S09 – Offset fachlich korrekt begrenzen und implementieren

- **Voraussetzungen:** S08, R08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorOperations.cpp`, `src/features/SketchProfiles.cpp`, `tests/integration/SketchBasicToolsTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Exakte Linien-/Kreis-/Arc-Offsets samt Konturanschlüssen und Offsetrichtung definieren.
  2. Ellipsenoffset nicht durch Halbachsenaddition ersetzen: vorerst mit klarer UnsupportedCurve-Meldung sperren, bis geeignete Kurvenrepräsentation spezifiziert ist.
  3. Selbstschnitt, zusammenfallende Innenoffsets und ungültige Distanz transaktional behandeln.
- **Abnahme:** Linie hält Normalabstand d, Kreis Radius r±d, kollabierender Innenoffset wird abgewiesen. Ellipse wird ausdrücklich als nicht unterstützt erklärt. Preview und Commit geometrisch identisch.
- **Grenze:** Keine neue Spline-Repräsentation in diesem Paket; späteres Surface-/Curve-Paket darf den Umfang erweitern.

## S10 – Trim, Extend und Split mit klarer Werkzeugmatrix liefern

- **Voraussetzungen:** S08, R08.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorOperations.cpp`, `tests/integration/SketchBasicToolsTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Unterstützung pro Kurventyp festlegen und UI entsprechend aktivieren; vorhandenen Linienweg robust vervollständigen.
  2. Schnittwahl, entstehende IDs und Übertragung oder Invalidierung von Constraints definieren.
  3. Tangentialfall, endpoint-nahe Schnitte, Nullsegmente und nicht vorhandene Schnittziele behandeln.
- **Abnahme:** Line/Line und freigegebene weitere Kombinationen liefern richtige Stücke; Unsupported-Kurve wird vor Mutation abgewiesen. Folgeconstraints werden korrekt erhalten oder lokalisiert reparaturbedürftig. Undo ist ein Schritt.
- **Grenze:** Keine Dokumentation als allgemeines Trim/Extend, solange nur Linien unterstützt sind.

## S11 – Skizzenfillet und Beziehungen nach Operationen erhalten

- **Voraussetzungen:** S10, S07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorOperations.cpp`, `src/application/SketchEditorRelations.cpp`, `tests/integration/SketchBasicToolsTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Fillet zwischen zwei Linien mit eindeutiger Seite/Radius und korrekter Arc-Tangentialität fertigstellen.
  2. Constraints entfernter/gekürzter Stücke semantisch zuordnen; unauflösbare Beziehungen nicht an zufällige Nachfolger hängen.
  3. Parallel-/kollinear-/zu-großer-Radius-Fälle mit Fehlerposition anzeigen.
- **Abnahme:** Rechtwinklige Linien erzeugen analytisch richtigen Kreisbogen und tangentiale Übergänge; große/negative Radien werden ohne Teiländerung abgewiesen. Solve und Undo bleiben konsistent.
- **Grenze:** Keine allgemeine Splineverrundung.

## S12 – Skizzenmuster und Spiegelbeziehungen ehrlich definieren

- **Voraussetzungen:** S08, S07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/application/SketchEditorConstruction.cpp`, `include/voelund/document/SketchGeometry.hpp`, `tests/integration/SketchConstructionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Für v1 bewusst zwischen Einmalkopie und assoziativem Muster entscheiden und Namen/Parameter entsprechend festlegen.
  2. Wenn assoziativ: Master-/Instanzreferenzen, Count/Spacing/Angle und Constraintübertragung als eigenes Datenobjekt spezifizieren, anschließend in kleinem Folgepaket implementieren.
  3. Nicht implementierte Persistenzsemantik darf nicht durch nur erzeugte Kopien als parametrisch ausgegeben werden.
- **Abnahme:** Änderung am Master wirkt nur bei ausdrücklich assoziativem Modus; Einmalkopie bleibt unabhängig. Anzahlgrenzen, zyklische Abhängigkeit und Save/Open sind geprüft.
- **Grenze:** Keine stillschweigende Ausweitung des Skizzenformats; neues Schema über D13-Migrationsregeln.

## S13 – Skizzen-UX und Auswahlmodi abschließen

- **Voraussetzungen:** S06, S07, S09, S10, S11, S12.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08, ADR 0012; `src/viewport/SketchBoxSelection.cpp`, `src/viewport/SketchEditorInteraction.cpp`, `src/desktop/SketchEditorToolController.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Window/Crossing-Auswahl, additive Auswahl, Escape-Abfolge und wiederholte Erzeugung eindeutig machen.
  2. Grid/Dimensionen/projizierte Geometrie/Auto-Constraints-Schalter tatsächlich mit Ansichts- oder Fachzustand verbinden.
  3. Fehler an betroffenen Elementen, numerische Eingabe und Tastaturfokus konsistent anzeigen.
- **Abnahme:** Alle vier Schalter verändern den benannten Zustand; Ansichtsänderungen erzeugen kein Dirty. Auswahl mit Ellipse/Arc und Constraint-Label kollidiert nicht. Escape beendet zuerst Vorschau, dann Werkzeug nach dokumentierter Regel.
- **Grenze:** Kein eigenes Skizzen-Ribbon: Skizzenwerkzeuge sitzen nach ADR 0012 in der Kontextleiste; keine versteckten Fachregeln in Eventhandlern.

## S14 – Skizzenumfang als Ganzes abnehmen und dokumentieren

- **Voraussetzungen:** S13, U12, R10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/sketch-data-model.md`, `docs/development-status.md`, `tests/integration/SketchEditorSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Werkzeug×Erzeugen×Preview×Edit×Undo×Save/Open×Snap×Label-Matrix abarbeiten.
  2. Regressionsfälle F05/F12–F15/F26/F28 aufnehmen und Unterschiede zwischen unterstützten/nicht unterstützten Operationen veröffentlichen.
  3. Referenzskizzen mit mehreren Regionen und projizierten Kanten durch die produktive Shell prüfen.
- **Abnahme:** A-SKETCH-01/02, A-REG-01/02 und A-TXN-03 bestehen. Kein aktives Werkzeug ohne vollständige Zeile in der Matrix. Bekannte Begrenzungen sind in UI und Datenmodelldokument gleich beschrieben.
- **Grenze:** Kein „Sketch fertig“ allein aus Backend-Smokes ableiten.
