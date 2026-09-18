# 04 – Eine Anwendungsschale und ein vollständiger Skizzenablauf

Die bestehende genehmigte Gestaltung bleibt erhalten. K03/K04/K07 sind Pflichtverträge. Dieses Kapitel baut kein zusätzliches Frontend, sondern führt vorhandene Funktionen und Darstellung zusammen. U11 wartet ausdrücklich auf die benötigten S-Pakete; die IDs sind keine starre numerische Reihenfolge.

**Prüfweg:** vorhandene `application.sketch_*`-Tests und `workspaces.module_registry`, neue fachliche Regressionen und ab U11 echte UI-Ereignisse. Bei Darstellung mit gleicher Fenstergröße/DPI visuell vergleichen; Bilder ersetzen keine Prüfung des tatsächlichen Dokumentzustands.

## U01 – Sichtbare Befehle mit tatsächlicher Funktion abgleichen

- **Voraussetzungen:** A08, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-ui-preview/PreviewWindow.cpp`, `apps/voelund-ui-preview/PreviewWindowRibbon.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Befehlstabelle mit ID, UI-Einstieg, Controller, Zustand und Abnahmetest anlegen.
  2. Nicht implementierte Buttons/Workspaces eindeutig deaktivieren oder ausblenden; Aktivierungsbedingungen für Skizzen-/Körper-/Flächenauswahl definieren.
  3. Feste Dokumenttitel und Timelineattrappen durch leeren oder tatsächlichen Zustand ersetzen.
- **Abnahme:** Kein aktiver Button ohne registrierte ausführbare Aktion. Extrude wird erst nach M04 aktiv produktiv angeboten. Nicht verfügbares Werkzeug verändert keine Auswahl/Dokumenthistorie.
- **Grenze:** Genehmigte Gestaltung und Navigation beibehalten; keine neue Oberflächenästhetik.

## U02 – Gemeinsames Shell-ViewModel definieren

- **Voraussetzungen:** U01, D04.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `include/voelund/desktop/MainWindow.hpp`, `apps/voelund-ui-preview/PreviewWindow.hpp`, `src/desktop/MainWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Verantwortlichkeiten beider Fenster in Shell, Dokumentaktionen, Toolhost, Tree/Timeline und Viewport zuordnen.
  2. Kleine read-only Projektion für Dokumenttitel, Dirty, aktuelle Auswahl, Befehlsverfügbarkeit und Toolzustand bereitstellen.
  3. Ownership und Lebensdauer von CadSession, Viewport und Panels im Composition Root dokumentieren.
- **Abnahme:** Header-/Komponentenskizze benennt für jeden Zustand genau einen Besitzer. Keine zweite CadSession pro Ansicht und keine direkten Featuremutatoren aus Widgets.
- **Grenze:** API-/Strukturpaket; keine gleichzeitige Migration aller UI-Komponenten.

## U03 – Genehmigte Gestaltung in gemeinsame Desktop-Komponenten verschieben

- **Voraussetzungen:** U02, A03.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-ui-preview/PreviewStyle.cpp`, `apps/voelund-ui-preview/PreviewIcons.cpp`, `apps/voelund-ui-preview/ViewCube.cpp`, `src/desktop/CMakeLists.txt`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Style/Icons/ViewCube als wiederverwendbare Darstellungskomponenten nach desktop bzw. zuständigem Viewportbereich extrahieren.
  2. Preview- und Produktfenster vorübergehend dieselben Komponenten verwenden lassen; keine Fachlogik übertragen.
  3. Screenshots bei gleicher Größe/DPI als visuelle Vergleichsbasis ablegen.
- **Abnahme:** Layout, genehmigte Farbgebung, Navigation und ViewCube bleiben visuell erhalten. Komponenten hängen nicht von PreviewWindow oder dessen Fake-Daten ab. Start-Smokes grün. Der Agent erstellt und betrachtet Vorher-/Nachherbilder gemäß Kapitel 10 selbst; keine routinemäßige Nutzerfreigabe für die Erhaltung der genehmigten Gestaltung.
- **Grenze:** Nur Darstellung; kein Parametermodell im Style-/Iconmodul.

## U04 – Ribbon an CommandRegistry anbinden

- **Voraussetzungen:** U02, U03, AI02.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/workspaces/CommandRegistry.cpp`, `src/modules/solid/SolidWorkspaceModule.cpp`, `src/desktop/WorkspaceToolbar.cpp`, `apps/voelund-ui-preview/PreviewWindowRibbon.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Ribbon-Elemente aus registrierten Befehlen und Workspace-Metadaten erzeugen/binden.
  2. Shortcuts und Kontextmenü verwenden dieselbe Command-ID und Zustandsprüfung.
  3. Aktivierung bei Auswahl-/Tool-/Dokumentwechsel aktualisieren; No-op-Routing auf Select entfernen.
  4. Gemeinsame zielgerichtete Application-Aktionen aus AI02 verwenden; Auswahl nur
     beim Erstellen des Requests auflösen. Keine parallele KI-/GUI-Fachlogik einführen.
- **Abnahme:** Ein Ribbon-Klick und der zugehörige Shortcut lösen identischen Application-Befehl aus. Unbekannte ID liefert Diagnose. Relation/Dimension/Trim/Offset gehen nicht versehentlich auf Select.
- **Grenze:** Noch nicht implementierte Fachtools bleiben deaktiviert.

## U05 – Generischen ToolPanelHost in die neue Shell integrieren

- **Voraussetzungen:** U04, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/ToolPanelHost.cpp`, `src/desktop/SketchEditorToolController.cpp`, `src/desktop/SketchExtrusionToolController.cpp`, `apps/voelund-ui-preview/PreviewWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. ToolPanelHost besitzt genau eine aktive Sitzung; Parameterpanel ist reine Bindung an deren State.
  2. OK/Cancel/Escape/Fokuswechsel gemäß K04 vereinheitlichen; Toolwechsel bei ungültiger Skizze blockiert mit Fehlermeldung.
  3. Panel-Layout der genehmigten Oberfläche verwenden, alte Controller über Application-Vertrag anschließen.
- **Abnahme:** A-UI-02 besteht für OK, Cancel, Escape und Toolwechsel. Cancel lässt Dokument/Undo/Dirty unverändert. Failed-Skizze startet kein Extrude-Panel.
- **Grenze:** Keine zweite temporäre Parametersammlung im PreviewWindow.

## U06 – Echte Dokumentprojektion für Titel, Tree und Eigenschaften anzeigen

- **Voraussetzungen:** U02, U05, D11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/ModelTreePanel.cpp`, `src/desktop/FeaturePropertiesPanel.cpp`, `src/desktop/MainWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Tree liest echte Body-/Feature-IDs, Namen, Fehler und Suppressionstatus.
  2. Titel aus Dokumentname und Dirty ableiten; Properties zeigen ausgewählte reale Parameter und Auswertungsstatus.
  3. Auswahl zwischen Tree/Viewport/Properties über typed SelectionRef synchronisieren.
- **Abnahme:** Neues/geladenes Dokument zeigt korrekte Inhalte statt Bracket/v12. Umbenennen und Undo aktualisieren alle Ansichten. Entfernte Auswahl wird bereinigt, ohne anderes Feature zu verändern.
- **Grenze:** Timeline-Bearbeitung folgt M15; hier nur Datenprojektion.

## U07 – New/Open/Save und Close-Schutz in der gemeinsamen Shell anschließen

- **Voraussetzungen:** U05, D03, D14.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/desktop/DocumentActions.cpp`, `src/desktop/MainWindow.cpp`, `apps/voelund-cad/main.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Dokumentaktionen mit dem sicheren Store verbinden und Fehler verständlich anzeigen.
  2. Dirty-New/Open/Close bietet Save/Discard/Cancel; fehlgeschlagener Save führt nicht trotzdem zum Schließen.
  3. Native Pfadkonvertierung für Windows und Unix eindeutig gestalten; Unicode-/Leerzeichenfälle verwenden.
- **Abnahme:** A-IO-01 und A-UI-03 bestehen: Cancel erhält Modell; Savefehler hält Fenster offen; Openfehler zerstört kein aktives Dokument; Save-As setzt Titel erst nach Erfolg.
- **Grenze:** Keine neue cloudbasierte Dokumentverwaltung.

## U08 – Sketch-Erzeugung zwischen beiden Shells vereinheitlichen

- **Voraussetzungen:** U05, D07.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-ui-preview/SketchBackend.cpp`, `src/desktop/SketchEditorCreation.cpp`, `src/application/SketchEditorSession.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Gemeinsamen Application-Erzeugungsbefehl mit semantischen Ergebnisrollen vorbereiten und beide UI-Wege darauf abbilden.
  2. Duplizierte Snap-/Constraint-Erzeugung aus UI entfernen; Renderingadapter erhalten nur Preview-/Result-Daten.
  3. Bestehende robuste Legacy-Fälle als Charakterisierungstests erhalten, Preview-Abweichungen ausdrücklich korrigieren.
- **Abnahme:** Rechteck, Circle, Arc und Ellipse aus beiden bisherigen Einstiegen erzeugen fachlich gleichen Editorzustand. Kein UI-Code berechnet IDs durch Einfügereihenfolge.
- **Grenze:** Vollständige Werkzeugmatrix folgt S01/S04–S06; kein dritter Bridge-Layer mit eigener Fachlogik.

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
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `tests/CMakeLists.txt`, `apps/voelund-cad/main.cpp`, `src/desktop/MainWindow.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Qt-Test klickt echte registrierte Aktionen, setzt Felder und bestätigt Panels nach Eventloopstart.
  2. Basisebene→Skizze→Constraint→OK→Edit→Cancel→Save/Open über tatsächliche UI ausführen.
  3. Rendering-Readiness, Dokumentrevision, Tree-Inhalte und Fehlerzustände prüfen; visuelle Bilder ergänzen fachliche Assertions.
- **Abnahme:** A-UI-01 Skizzenteil und A-UI-02/03 bestehen. Absichtlich abgetrennter Handler macht Test rot. Extrude-Teil wird in M04 ergänzt. A-VIS-01 bestanden: Bilder tatsächlich selbst betrachtet, Layout-/Overlayfehler korrigiert und mit neuer Aufnahme nachgeprüft.
- **Grenze:** Kein Test, der nur Backendmethoden aufruft und danach Fenstersichtbarkeit bestätigt.

## U12 – Eine produktive Anwendung und einen Composition Root festlegen

- **Voraussetzungen:** U03, U04, U05, U06, U07, U11.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `apps/voelund-cad/CMakeLists.txt`, `apps/voelund-ui-preview/CMakeLists.txt`, `src/desktop/MainWindow.cpp`, `apps/voelund-ui-preview/PreviewCanvas.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Produkt `voelund-cad` mit genehmigter gemeinsamer Shell bauen und installieren.
  2. Preview auf optionalen Darstellungs-/Entwicklungsharness ohne eigene Fachlogik reduzieren oder nach Parität entfernen.
  3. Fake-Canvas und verwaiste Handler entfernen; verwendete Screenshots/Doku auf produktiven Einstieg umstellen.
- **Abnahme:** Installiertes Produkt startet dieselbe getestete Shell. Kein zweiter SketchBackend mit eigenen Regeln, keine Fake-Timeline. Ein kompletter New/Sketch/Save/Open-Fluss besteht.
- **Grenze:** Keine Löschung der Vergleichsbasis vor belegter Funktions-/Gestaltungsparität.

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
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `src/viewport/SketchBoxSelection.cpp`, `src/viewport/SketchEditorInteraction.cpp`, `src/desktop/SketchEditorToolController.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Window/Crossing-Auswahl, additive Auswahl, Escape-Abfolge und wiederholte Erzeugung eindeutig machen.
  2. Grid/Dimensionen/projizierte Geometrie/Auto-Constraints-Schalter tatsächlich mit Ansichts- oder Fachzustand verbinden.
  3. Fehler an betroffenen Elementen, numerische Eingabe und Tastaturfokus konsistent anzeigen.
- **Abnahme:** Alle vier Schalter verändern den benannten Zustand; Ansichtsänderungen erzeugen kein Dirty. Auswahl mit Ellipse/Arc und Constraint-Label kollidiert nicht. Escape beendet zuerst Vorschau, dann Werkzeug nach dokumentierter Regel.
- **Grenze:** Kein neues Ribbon-Design und keine versteckten Fachregeln in Eventhandlern.

## S14 – Skizzenumfang als Ganzes abnehmen und dokumentieren

- **Voraussetzungen:** S13, U12, R10.
- **Pflichtlektüre/Einstieg:** Kapitel 01 betreffende F-Befunde, Kapitel 02 K01–K08; `docs/sketch-data-model.md`, `docs/development-status.md`, `tests/integration/SketchEditorSessionTests.cpp`. Nach A03 ggf. ausgegliederte Nachfolgesymbole lesen.
- **Auftrag:**
  1. Werkzeug×Erzeugen×Preview×Edit×Undo×Save/Open×Snap×Label-Matrix abarbeiten.
  2. Regressionsfälle F05/F12–F15/F26/F28 aufnehmen und Unterschiede zwischen unterstützten/nicht unterstützten Operationen veröffentlichen.
  3. Referenzskizzen mit mehreren Regionen und projizierten Kanten durch die produktive Shell prüfen.
- **Abnahme:** A-SKETCH-01/02, A-REG-01/02 und A-TXN-03 bestehen. Kein aktives Werkzeug ohne vollständige Zeile in der Matrix. Bekannte Begrenzungen sind in UI und Datenmodelldokument gleich beschrieben.
- **Grenze:** Kein „Sketch fertig“ allein aus Backend-Smokes ableiten.
