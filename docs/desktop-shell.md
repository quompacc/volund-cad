# Desktop-Shell: Komponenten und Besitz (U02)

Stand 04.10.2026. Zielbild: [ADR 0012](adr/0012-ui-shell-concept.md). Dieses Dokument legt
fest, welcher Teil welchen Zustand besitzt. Jeder Zustand hat genau einen Besitzer; alle
anderen lesen ihn über die Shell-Projektion oder bekommen ihn als Argument.

## Composition Root

`voelund::desktop::startProduct` ([ProductShell.hpp](../include/voelund/desktop/ProductShell.hpp),
U12) ist der Composition Root. Er beginnt ein neues Dokument in der übergebenen
`CadSession`, erzeugt genau ein `MainWindow`, zeigt es und wendet die Startoptionen an:
Claude-Zugang (`--mcp`), Wiederherstellung, Datei aus der Befehlszeile.
`configureProductApplication` setzt vorher Name, Organisation, Desktop-Datei und
Oberflächenformat.

`apps/voelund-cad/main.cpp` ruft nur diese beiden Funktionen auf (plus die Smoke-Modi).
Die UI-Ablauftests (`desktop.ui_flow`) starten die Shell über denselben Weg, also genau
die Shell des Produkts. Die Session lebt länger als das Fenster und wird nur als Referenz
weitergegeben. Es gibt keine zweite Session pro Ansicht.

`MainWindow` baut die Shell zusammen und besitzt über `MainWindow::Impl` (in
`MainWindowState.hpp`) alle Panels, Controller, Aktionen und die Registry. Qt-Widgets
gehören über die Parent-Kette dem Fenster; Controller und Registry gehören dem `Impl` als
`unique_ptr` bzw. Wert. Sie leben genau so lange wie das Fenster.

## Zustand und Besitzer

| Zustand | Besitzer | Lesen über |
|---|---|---|
| Dokument, Features, Historie, Speicherzustand, Datei | `CadSession` | `ShellState` (Titel, Undo/Redo), Panels |
| Gewähltes Feature, Skizzenelement, Skizzenbereiche | `CadSession` | `ShellState::selection`, `feature` |
| Gepickte Basisebene, gepickte ebene Fläche (nur Ansicht) | `MainWindow::Impl` (`selectedDatum`, `selectedPlanarFace`) | `ShellInputs` → `ShellState` |
| Aktives Werkzeug | Werkzeug-Controller (`SketchEditorToolController`, `SketchExtrusionToolController`, Transform-Aktion) | `ShellInputs::tool` → `ShellState::tool` |
| Werkzeugparameter während der Bearbeitung | jeweilige Werkzeugsitzung; `ToolPanelHost` zeigt sie nur an | Panel-Bindung |
| Befehle und ihre IDs | `CommandRegistry` (Module) und Shell-Aktionen mit `commandId` | Toolbars, Menü; später Werkzeugleiste, Kontextleiste, Befehlssuche (U04) |
| Befehlsverfügbarkeit | abgeleitet aus `ShellState` (`canSketch`, `canExtrude`, `canEditSketch`, `canUndo`, `canRedo`) | `isEnabled` der Befehle |
| Kamera, Sichtbarkeit, Raster, Darstellung | `CadViewport` (Ansichtszustand, kein Undo, kein Dirty) | Viewport-API; Baum-Augen in U06 |
| Fenstertitel | abgeleitet: `ShellState::windowTitle()` | `DocumentActions::syncWindowTitle` |

## Regeln

- Widgets ändern das Dokument nur über `CadSession`-Befehle (Command-Ergebnis nach K03).
  Sie rufen keine Featuremutatoren direkt auf und halten keine eigene Kopie von
  Dokumentdaten.
- Die Verfügbarkeit eines Befehls kommt aus `ShellState`, nicht aus dem Zustand eines
  Widgets. Neue Auswahlarten erweitern `ShellSelection`, nicht einzelne Widgets.
- Ansichtspicks sind spezifischer als die Dokumentauswahl. Die Reihenfolge ist:
  Basisebene, Fläche, Skizzenbereich, Skizzenelement, Skizze, Feature.
- Ein offener Skizzeneditor oder eine offene Extrusion sperren andere Werkzeuge,
  Grundkörper und das Dokument-Undo, bis OK, Abbrechen oder Escape (K04, U05). Die Karte
  rechts zeigt entweder das aktive Werkzeug oder die Eigenschaften des gewählten Features.
- U14: Alle Shell-Teile liegen als Karten über dem Viewport (`OverlayCard`, `FloatingCard`,
  `ToolRail`, `ContextBar`, `ViewCube`). Ihre Plätze stehen fest in `overlay::`
  (`OverlayCard.hpp`); neue Karten bekommen dort einen Platz, statt sich selbst zu suchen.
- Nach jeder Dokumentänderung ruft der Besitzer der Aktion `refreshScene()` auf. Es
  verwirft Ansichtspicks und aktive Werkzeuge und synchronisiert alle Ansichten.

## Zuordnung zum Zielbild

| Bereich (ADR 0012) | Komponente heute | Paket |
|---|---|---|
| Kopfzeile | `ShellHeader`: Projektkarte oben links (Dateimenü, Dokument, Speicherzustand) und Kopfkarte oben rechts (Undo/Redo, Suchfeld, Werkzeugleisten- und Theme-Schalter, Claude) | U03, U04, U14 erledigt |
| Modellbaum | `ModelTreePanel` in der Karte `modelCard` links | U06, U14 |
| Werkzeugleiste, Kontextleiste, Befehlssuche | `ToolRail` (Gruppen mit Flyouts), `ContextBar` (`ContextCommands`), `CommandPalette` auf `CommandActions` | U04 erledigt, U14 |
| Werkzeugpanel | `ToolPanelHost` als schwebende `FloatingCard` | U05 erledigt |
| Eigenschaften | `FeaturePropertiesPanel` in einer `FloatingCard` am selben Platz | U05 erledigt |
| Timeline | `TimelineBar` als Karte unten mittig, Symbol-Chips (Schritte öffnen ihren Editor) | U06 erledigt, U14, M15 (Bearbeitung) |
| Parameter | – | PAR02 |
| Viewport | `CadViewport` | U03 (Farben), U06 (Ursprünge, Raster) |
