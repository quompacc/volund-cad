# UI-Konzept (UX01)

Stand 04.10.2026. Teil 1 ist die Bestandsaufnahme. Teil 2, das Zielbild, wird mit dem
Nutzer erarbeitet und erst nach seiner ausdrücklichen Freigabe als ADR festgehalten.
Aufnahmen liegen in `artifacts/ux01-inventory/` (nicht versioniert). Erzeugt werden sie
mit `voelund_ui_inventory_capture` und dem Smoke-Test der Vorschau. Aktuelle Aufnahmen des
Produkts (hell/dunkel, normale und Mindestgröße) erzeugt seit U12 `desktop.ui_flow` über den
Composition Root des Produkts, mit `VOELUND_UI_FLOW_ARTIFACT_DIR=<verzeichnis>`.

## Teil 1 – Bestandsaufnahme

### Zwei Oberflächen nebeneinander

| | Produkt `voelund-cad` | Vorschau `voelund-ui-preview` |
|---|---|---|
| Rahmen | QMainWindow, nativer Stil (hell auf Windows, Breeze auf KDE) | Eigenes dunkles Grün/Gold-Theme (QSS), feste Höhenaufteilung |
| Befehle | Nur Menü „File“; Toolbars History, SOLID (jeder Befehl doppelt als Knopf und im Menü), Connection, Transform/Snap | Kopfzeile, Arbeitsbereichs-Tabs, Ribbon mit Gruppen und Dropdowns |
| Modell | Dock „Model“: Wurzel „Body“, flache Featureliste ohne Symbole, ohne Kontextmenü | „Browser“ mit Filter, Symbolen und „Origin“-Eintrag (ohne Funktion) |
| Werkzeuge | Docks „Properties“ und „Tool“ übereinander als Reiter | Schwebender Inspektor im Viewport (z. B. Extrude) |
| Navigation | Rechts drehen, Mitte verschieben, Rad zoomen; kein ViewCube, keine Standardansichten | ViewCube mit Top/Front/Right/ISO/FIT/Ortho |
| Historie | – | Timeline-Leiste mit festem Platzhaltertext |
| Funktion | Real: Box, Zylinder, Skizze, Extrusion, Speichern, Undo, Reparatur | Nur Skizze echt; Extrude-OK, Revolve, Hole, Fillet, Chamfer, Shell, Joint sind Attrappen |

Im Plan galt bisher die Gestaltung der Vorschau als „genehmigt“ (dunkle Palette nach
Völund Vault, Fusion-ähnlicher Rahmen). Sie wurde aber nie ins Produkt übernommen. Seit
dem 04.10.2026 gilt sie nicht mehr als Grundlage; das Zielbild entsteht neu in UX01.

### Brüche im Arbeitsablauf (Produkt)

1. **Kein klarer Einstieg.** Die Anwendung startet mit Box und Zylinder. Ein leeres
   Dokument gibt es nur über File → New, und es gibt keine Startansicht.
2. **Doppelte Wege.** „Sketch“ und „Sketch Rectangle“ (alter Push/Pull-Weg) stehen
   nebeneinander. Jeder Befehl erscheint zweimal. Die Vorschau hat einen zweiten
   Skizzen-Erzeugungsweg (Befund F11).
3. **Auswahl ist schwer verständlich.** Basisebenen liegen neben Körpern und fangen Klicks
   ab (Aufnahme 02: Klick auf die Box traf die XZ-Ebene; der Editor startete auf der
   falschen Ebene). Profile wählt man durch Klick auf eine Skizzenkante, die nur sichtbar
   ist, wenn die Skizze im Baum gewählt ist (Aufnahmen 04/05).
4. **Modellbaum ohne Bedienung.** Keine Symbole, kein Umbenennen, Löschen, Ausblenden oder
   Unterdrücken, kein Kontextmenü. Fehlerzustände erscheinen nur als Text-Suffix
   „[error]“/„[blocked]“.
5. **Werkzeuge und Eigenschaften konkurrieren.** Das Werkzeugpanel liegt als zweiter
   Reiter hinter „Properties“. Der Skizzeneditor bietet rund 45 Knöpfe in fünf Reitern
   gleichzeitig an.
6. **Globale statt objektbezogener Einstellungen.** „Connection“ ändert immer den
   letzten Zylinder, nicht den gewählten, und jede Änderung ist ein Undo-Schritt.
   Snap-Werte stehen dauerhaft in der Toolbar.
7. **Navigation.** Es gibt keinen ViewCube, keine Standardansichten, kein „Alles
   zeigen“. Die Kamera kehrt nach einer Skizze nicht zurück.
8. **Tastatur.** Es gibt keine Entf-Taste, kein Escape außerhalb von Werkzeugen und
   keine Ansichtstasten. Strg+Z/Y sind teils doppelt belegt. Undo/Redo sind gesperrt,
   wenn nichts gewählt ist.
9. **Darstellung.** Gemischt: heller nativer Stil mit dunklen Fehlerfarben, grün-dunkler
   Viewport, kein einheitliches Farb- und Symbolsystem. Die Texte sind nur teilweise
   übersetzbar; Deutsch gibt es nicht.
10. **Fehler und Reparatur.** Die Reparatur ist nur über Toolbar-Knöpfe und Dialoge
    erreichbar. Der Status „Last valid geometry …“ in der Statusleiste ist schwer zu
    verstehen.

### Bereits festgelegte Nutzervorgaben

- **Basisebenen:** An das Objekt gebunden (Ursprung je Körper bzw. Komponente) und im
  Modellbaum ein- und ausblendbar. Im leeren Dokument sind sie sichtbar, neben Körpern
  nur dezent (Rückmeldung zu V01b vom 04.10.2026).
- **Parametertabelle:** Mit einem Klick erreichbar, Eingabe `=A` in Maßfeldern
  ([Konzept](parameter-table.md)).
- **Mehrere Körper** direkt im Dokument, ohne vorher eine Komponente anzulegen
  (A-BODY-03).
- **Abstimmung:** Sichtbare Änderungen werden vorher mit dem Nutzer abgestimmt;
  Aufnahmen werden zur Durchsicht vorgelegt.

### Fragen für das Zielbild

1. **Vorbild:** Woran soll sich die Bedienung orientieren? Fusion 360 (Ribbon,
   Browser, Timeline, schwebende Panels), Onshape, SolidWorks oder FreeCAD (Workbenches)?
   Was gefällt dir dort, was stört?
2. **Erscheinung:** Dunkles Theme wie in der Vorschau (Grün/Gold), neutral dunkel oder
   hell? Gilt die Vorschau-Optik überhaupt noch als Richtung?
3. **Befehle:** Ribbon mit Gruppen oder schlanke Toolbar mit Kontextpanels? Arbeitsbereiche
   (Solid, Skizze, später Baugruppe) als Tabs?
4. **Historie:** Timeline unten wie in Fusion oder Feature-Baum wie in FreeCAD und
   SolidWorks, oder beides?
5. **Panels:** Werkzeuge als schwebender Inspektor am Viewport oder fest angedockt rechts?
6. **Sprache:** Deutsch als Oberflächensprache, Englisch oder umschaltbar?
7. **Skizzenmodus:** Eigene Werkzeugleiste beim Skizzieren; Constraints als Symbole am
   Element oder als Panel?

## Teil 2 – Zielbild

Vom Nutzer am 04.10.2026 freigegeben; verbindlich festgehalten in
[ADR 0012](adr/0012-ui-shell-concept.md). Grundlage ist der klickbare Prototyp
„Völund UI-Konzept“ (Quelle `artifacts/ux01-mockup/`, nicht versioniert).

### Rückmeldung des Nutzers

- „Sehr einfach ohne viel Lärm.“ Die Grundrichtung wird so umgesetzt.
- Die senkrechte Werkzeugleiste bleibt, ausblendbar.
- Die Timeline wird ausdrücklich gewünscht.
- Das Raster auf XY ist zu Beginn nützlich und lässt sich ausblenden.
- Parameter gelten dokumentweit. Eine dateiübergreifende Bibliothek ist vorerst nicht
  vorgesehen.
- Bedingung: Völund darf nicht zum Spielzeug-CAD werden; es bleibt vollparametrisch
  (siehe ADR 0012).
- Ein endgültiges Urteil fällt erst beim Modellieren eines echten Teils im Produkt. Die
  Umsetzung bleibt deshalb in kleinen, vorführbaren Schritten.

- Nach U05: Die Werkzeugleiste soll sich dem aktiven Modus anpassen und die Werkzeuge der
  gerade genutzten Funktion zeigen. Im Skizzenmodus sind das Skizzenelemente,
  Beziehungen, Bemaßungen usw. Umsetzung in U13.

### Ablauf im Zielbild

1. Ein leeres Dokument zeigt Ursprung, Ebenen und Raster. Ein Hinweis lautet „Ebene
   oder Fläche anklicken“.
2. Ist eine Ebene oder Fläche gewählt, zeigt die Kontextleiste „Skizze erstellen“
   (hervorgehoben), „Ebene versetzen“ und „Ausblenden“.
3. Im Skizzenmodus sitzen die Skizzenwerkzeuge in der Kontextleiste. Das Panel zeigt
   Bestimmtheit und Maße, `=A` bindet ein Maß an einen Parameter. „Skizze fertig“
   schließt den Modus.
4. Ein Klick auf einen Bereich der Skizze wählt ihn; danach sind „Extrudieren“ und
   „Drehen“ möglich.
5. Das Extrusionspanel bietet Distanz (auch `=D`), Richtung (entlang/entgegen der Normale,
   symmetrisch: Distanz = Gesamtlänge) und Ergebnis (neuer Körper, vereinigen, abziehen,
   schneiden) mit Live-Vorschau sowie OK/Abbrechen (M02, Nutzerentscheidung 11.10.2026).
6. Danach stehen der Körper mit Ursprung, Skizze und Feature im Baum, und die Timeline
   zeigt „Skizze 1“ und „Extrusion 1“. Ein Klick öffnet den jeweiligen Schritt.

### Zuordnung zu Paketen

| Paket | Inhalt im Zielbild |
|---|---|
| U01 | Befehlstabelle des Produkts; welche Befehle Werkzeugleiste und Kontextleiste zeigen |
| U02 | Shell-Projektion: Titel, Speicherzustand, Auswahl, Befehlsverfügbarkeit, Werkzeugzustand |
| U03 | Gestaltung als Desktop-Komponenten: Farben hell/dunkel, Schriften, Symbole, ViewCube, Grundlayout |
| U04 | Werkzeugleiste, Kontextleiste und Befehlssuche an der CommandRegistry |
| U05 | Schwebendes Werkzeugpanel mit OK/Abbrechen/Escape |
| U06 | Modellbaum mit Ursprüngen und Augen-Schaltern, Raster, Timeline als Projektion |
| V01 | UI-Test des ganzen Ablaufs (V01c) in der neuen Oberfläche |
| PAR02 | Parameterpanel rechts, `=Name` in Maßfeldern |
| M15 | Timeline mit Rückrollmarke, Fehlern und Umordnen |

## Teil 3 – Befehlstabelle (U01)

Jede sichtbare Aktion trägt eine stabile Command-ID (Qt-Property `commandId`). Der Test
`desktop.command_inventory` prüft, dass die sichtbaren Aktionen genau dieser Tabelle
entsprechen. Registry-Befehle verwenden ihre registrierte ID. Die heutigen Einstiege sind
Menü und Toolbars; Werkzeugleiste, Kontextleiste und Befehlssuche nach ADR 0012 binden
in U04 an dieselben IDs.

| Command-ID | Einstieg (seit U04) | Controller | Aktiv, wenn | Test |
|---|---|---|---|---|
| `document.new` | Dateimenü, Strg+N, Suche | DocumentActions | immer | document_actions |
| `document.open` | Dateimenü, Strg+O, Suche | DocumentActions | immer | document_actions |
| `document.save` | Dateimenü, Strg+S, Suche | DocumentActions | immer | document_actions |
| `document.save_as` | Dateimenü, Suche | DocumentActions | immer | document_actions |
| `document.duplicate` | Dateimenü, Suche | DocumentActions | immer | document_actions |
| `history.undo` | Kopfzeile, Strg+Z, Suche | MainWindow | Historie hat einen Schritt | command_inventory |
| `history.redo` | Kopfzeile, Strg+Y, Suche | MainWindow | Historie hat einen Schritt | command_inventory |
| `repair.references` | Kontextleiste (Skizze), Suche; später am Feature (U10) | ReferenceRepairDialog | Skizze gewählt | reference_repair_flow |
| `repair.all` | Suche; später am Feature (U10) | DocumentRepairDialog | Dokument hat Features | document_repair_flow |
| `transform.snap` | Kontextleiste (Verschieben/Drehen), Suche | Viewport | immer | command_surfaces |
| `view.theme` | Kopfzeile (Sonne/Mond), Suche | MainWindow | immer | command_inventory |
| `body.rename` | Kontextleiste (Körper), Doppelklick/F2 auf die Körperzeile, Suche | CadSession::renameBody | Körperzeile im Baum gewählt (M01); ein Rückgängig-Schritt | body_flow |
| `body.visibility` | Kontextleiste (Körper), Auge im Baum, Suche | CadSession::setBodyHidden | Körperzeile im Baum gewählt (M01); gespeichert, „geändert“, kein Rückgängig-Schritt | body_flow |
| `view.command_search` | Kopfzeile (Suchfeld), Strg+K | CommandPalette | immer | command_surfaces |
| `view.tool_rail` | Kopfzeile (Werkzeugleiste), Suche | MainWindow | immer | command_surfaces |
| `view.grid` | Suche, Skizzenpanel (Raster), Modellbaum (Auge) | MainWindow | immer; Ansichtszustand ohne Undo und ohne „geändert“ | sketch_view_switches |
| `solid.create.box` | Werkzeugleiste, Kontextleiste (leer), Suche | CadSession | immer | command_inventory, command_surfaces |
| `solid.create.cylinder` | Werkzeugleiste, Kontextleiste (leer, Feature), Suche | CadSession | Geometrie aktuell; verbindet sich mit dem gewählten Körper, ohne Auswahl ein neuer Körper im Ursprung (M01, Nutzerentscheidung 10.10.2026) | command_inventory |
| `solid.create.sketch` | Werkzeugleiste, Kontextleiste (Ebene/Fläche, primär), Suche | SketchEditorToolController | Ebene oder ebene Fläche gewählt, Modell aktuell | base_planes |
| `solid.create.extrude_profiles` | Werkzeugleiste, Kontextleiste (Bereich, primär), Suche | SketchExtrusionToolController | Skizzenbereich gewählt | region_selection_acceptance |
| `solid.modify.transform` | Werkzeugleiste („Bewegen“), Kontextleiste (Feature), M, Suche | Viewport-Gizmo | getrennter Körper gewählt | viewport_startup |
| `solid.modify.edit_sketch` | Werkzeugleiste („Bearbeiten“), Kontextleiste (Skizze, primär), Suche | SketchEditorToolController | Skizze gewählt | support_binding_flow |
| `solid.modify.push_pull` | Werkzeugleiste („Ziehen“), Kontextleiste (ebene Fläche), Suche | PushPullToolController (T04) | ebene Fläche gewählt, Modell aktuell | push_pull_panel |
| `assistant.mcp` | Kopfzeile (Claude-Chip), Suche | McpAccess | immer; zeigt an/aus | mcp_shell |
| `assistant.mcp_copy_command` | Kopfzeile (Claude-Chip), Suche | McpAccess | Zugang an | mcp_shell |
| `assistant.mcp_new_token` | Kopfzeile (Claude-Chip), Suche | McpAccess | KI-Funktionen erlaubt | command_inventory |
| `assistant.panel` | Kopfzeile (Klick auf den Claude-Chip), Suche | AssistantPanel | KI-Funktionen erlaubt | assistant_panel |
| `assistant.allow` | Kopfzeile (Claude-Chip), Suche | McpAccess | nicht durch `VOELUND_DISABLE_AI` gesperrt; aus: kein Port, kein Claude Code, Chip ausgeblendet | mcp_shell |

Skizzenbefehle (U13): Solange der Skizzeneditor offen ist, zeigt die Werkzeugleiste statt der
Volumenwerkzeuge die Skizzenwerkzeuge. Jedes ist ein Befehl mit Command-ID, den auch Suche und
Kürzel erreichen; außerhalb einer Skizze sind sie gesperrt.

Anordnung in der Werkzeugleiste (S14, Nutzerentscheidung 10.10.2026): je Familie ein Knopf.
Er zeigt die zuletzt benutzte Variante, sein Pfeil öffnet die anderen. Abschnitte und
Familien: **Zeichnen** (Auswahl · Linie/Polylinie · Rechteck/Mittelpunkt-Rechteck ·
Kreis/Ellipse · Mittelpunkt-/Dreipunktbogen · Langloch · Polygon), **Beziehungen**
(Richtung · parallel/rechtwinklig/kollinear · Punkte · fix/gleich/tangential/konzentrisch),
**Bemaßungen** (Abstände · Durchmesser · Winkel), **Bearbeiten** (stutzen/verlängern/teilen ·
Verrundung · Versatz · Spiegeln/Muster · Punkt sperren/freigeben · Konstruktion ·
Löschen/Bedingung entfernen). Rückgängig/Wiederholen im Werkzeug: Strg+Z/Strg+Y und Suche.
Der Dreipunktbogen nimmt Anfang, Ende und dann einen Punkt auf dem Bogen.

| Gruppe | Command-IDs | Kürzel |
|---|---|---|
| Zeichnen | `sketch.tool.select`, `.line`, `.polyline`, `.rectangle`, `.circle`, `.center_arc`, `.three_point_arc`, `.ellipse`, `.slot`, `.polygon`, `.center_rectangle` | S, L, P, R, C, A, T, E, N, G, Umschalt+R |
| Linien | `sketch.horizontal`, `.vertical`, `.free`, `.parallel`, `.perpendicular`, `.collinear` | – |
| Punkte | `sketch.coincident`, `.horizontal_points`, `.vertical_points`, `.symmetric`, `.midpoint`, `.perpendicular_bisector`, `.point_on_object` | – |
| Beziehungen | `sketch.fixed`, `.equal`, `.tangent`, `.concentric` | – |
| Bemaßungen | `sketch.diameter`, `.point_distance`, `.point_line_distance`, `.point_distance_x`, `.point_distance_y`, `.angle` | – |
| Bearbeiten | `sketch.construction`, `.lock_point`, `.unlock_point`, `.remove_constraint`, `.undo_sketch`, `.redo_sketch`, `.trim`, `.extend`, `.split`, `.fillet`, `.offset`, `.mirror`, `.linear_pattern`, `.circular_pattern`, `.edit_pattern`, `.dissolve_pattern`, `.delete` | Strg+Z/Strg+Y für Rückgängig/Wiederholen im Werkzeug; Entf löscht |
| Anzeige (S13) | `sketch.show_dimensions`, `.show_projected`, `.auto_relations` (dazu `view.grid`) | Suche und Skizzenpanel; Ansichtszustand ohne Undo und ohne „geändert“ |

Bedienregeln im Skizzeneditor (S13, Nutzerentscheidung 09.10.2026):

- **Escape** geht schrittweise zurück: laufendes Ziehen abbrechen, dann Vorschaupunkte
  zurücknehmen, dann vom Zeichenwerkzeug zur Auswahl, dann die Auswahl aufheben. Escape
  verwirft die Skizze nie.
- **Abbrechen** fragt bei Änderungen „Änderungen an der Skizze verwerfen?“ („Verwerfen“ /
  „Weiter bearbeiten“).
- **Enter** übernimmt im Eingabefeld den Wert und beendet im Ansichtsfenster eine laufende
  Polylinie. **Strg+Enter** oder OK übernimmt die Skizze. In anderen Werkzeugen bleibt
  Enter = OK.
- **Wiederholtes Zeichnen:** Das Werkzeug bleibt nach einem Element aktiv; die Polylinie
  setzt am letzten Endpunkt fort, bis Enter oder Escape sie beendet.
- **Rahmenauswahl:** links → rechts ein Fenster (durchgezogen: Elemente ganz im Rahmen),
  rechts → links ein Kreuzen-Rahmen (gestrichelt: Elemente, die er berührt). Es werden
  ganze Elemente gewählt, Kurven zählen über ihren Verlauf. Strg fügt hinzu; ein leerer
  Rahmen ohne Strg hebt die Auswahl auf.
- **Element vor Beschriftung:** Ein Klick wählt das Element unter der Beschriftung. Ein
  Doppelklick auf ein Maß öffnet den Wertdialog, auf eine Beziehung wählt er sie (orange);
  Entf oder „Bedingung entfernen“ löscht sie. Gelbe Beschriftungen sind überflüssig.
- **Fehler** stehen in der Statuszeile; Elemente, die eine Ablehnung verursachen, sind rot,
  bis sich die Auswahl ändert.
- **Beschriftungen** (U11) stehen 12 px rechts neben ihrem Punkt. Mehrere am selben Punkt
  stehen untereinander, in jeder Zoomstufe. Die Grundfarbe folgt dem Theme (dunkelblau auf
  hell, hellblau auf dunkel). Getroffen wird der Textkasten selbst.
- **Bearbeiten einer Skizze** (U11) rückt sie in die freie Fläche zwischen Palette und
  Panel. Ist das Fenster dafür zu schmal, steht sie rechts neben der Palette.
- **Schmale Fenster** (U11): Unter 1250 px Breite zeigen Suche und Werkzeugleisten-Schalter
  in der Kopfzeile nur ihr Symbol. Der Modellbaum ist mindestens 190 px breit.

Die Kontextleiste zeigt je Auswahl (`ContextCommands`):

| Auswahl | Titel | Befehle (primär fett) |
|---|---|---|
| nichts | Ebene oder Fläche anklicken | Skizze, Quader, Zylinder |
| Basisebene | XY-/XZ-/YZ-Ebene | **Skizze** |
| ebene Fläche | Ebene Fläche | **Skizze**, Drücken/Ziehen |
| Skizzenbereich | Skizzenbereich | **Extrudieren**, Bearbeiten |
| Skizze, Skizzenelement | Skizze | **Bearbeiten**, Referenzen reparieren |
| Feature | Quader/Zylinder/Extrusion | Bewegen, Zylinder |
| Werkzeug Verschieben/Drehen | Verschieben/Drehen | Fangen, Bewegen, Fangwerte |
| Skizzeneditor, Extrusion, Drücken/Ziehen | – (Leiste aus; das Panel trägt die Bedienung) | – |

Bereinigt in U01:

- „Sketch Rectangle“ (alter Weg Rechteck + Herausziehen) ist entfernt. Skizzen entstehen
  nur noch über „Sketch“, extrudiert wird über gewählte Bereiche.
- Die globale Toolbar „Connection“ ist entfernt. Sie änderte immer den letzten Zylinder.
  Die Verbindungsart steht jetzt in den Eigenschaften des gewählten Zylinders; neue
  Zylinder vereinigen.
- Das Produkt startet mit einem leeren Dokument statt mit Box und Zylinder.
- Undo/Redo hängen nur noch an der Historie, nicht mehr an einer Auswahl.
- Die Viewport-Interaktion des alten Rechteckwegs (`SketchInteraction`,
  `beginRectangleSketch`) ist mit U12a entfernt.
