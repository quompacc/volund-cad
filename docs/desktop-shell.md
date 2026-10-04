# Desktop-Shell: Komponenten und Besitz (U02)

Stand 04.10.2026. Zielbild: [ADR 0012](adr/0012-ui-shell-concept.md). Dieses Dokument legt
fest, welcher Teil welchen Zustand besitzt. Jeder Zustand hat genau einen Besitzer; alle
anderen lesen ihn über die Shell-Projektion oder bekommen ihn als Argument.

## Composition Root

`apps/voelund-cad/main.cpp` erzeugt genau eine `CadSession` und ein `MainWindow`. Die
Session lebt länger als das Fenster und wird nur als Referenz weitergegeben. Es gibt keine
zweite Session pro Ansicht.

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
- Nach jeder Dokumentänderung ruft der Besitzer der Aktion `refreshScene()` auf. Es
  verwirft Ansichtspicks und aktive Werkzeuge und synchronisiert alle Ansichten.

## Zuordnung zum Zielbild

| Bereich (ADR 0012) | Komponente heute | Paket |
|---|---|---|
| Kopfzeile | Toolbar History, Menü File | U03 (Layout), U04 (Befehlssuche) |
| Modellbaum | `ModelTreePanel` | U06 |
| Werkzeugleiste, Kontextleiste | `WorkspaceToolbar` | U04, liest `ShellState::selection` |
| Werkzeugpanel | `ToolPanelHost` (Dock) | U05 (schwebend) |
| Eigenschaften | `FeaturePropertiesPanel` (Dock) | U05 (geht im Werkzeugpanel auf) |
| Timeline | – | U06 (Projektion), M15 (Bearbeitung) |
| Parameter | – | PAR02 |
| Viewport | `CadViewport` | U03 (Farben), U06 (Ursprünge, Raster) |
