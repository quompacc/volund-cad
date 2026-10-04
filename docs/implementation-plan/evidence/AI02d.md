# AI02d – Anbindung der Shell

**Status:** abgenommen am 04.10.2026 nach grünem CI-Lauf 382; damit ist AI02 abgeschlossen. Teilplan: [AI02-plan.md](../AI02-plan.md).

## Umsetzung

- **Requests aus der Shell:** Alle ändernden Shell-Befehle laufen über
  `executeAction`. Die Auswahl bzw. der gespeicherte Werkzeugstand bereitet den Request
  vor; danach entscheiden nur noch das explizite Ziel und der Stand.
  - Box und Zylinder (`MainWindowCommands`): `CreateBox` mit 20 × 20 × 40,
    `CreateCylinder` auf dem letzten Feature mit `join`, wie bisher.
  - Eigenschaftskarte (`MainWindowParameters`): neue Aktionen `EditBoxParameters`,
    `EditCylinderParameters` und `EditExtrusionParameters` mit den vollständigen Parametern
    der Karte, Ziel ist das beim Übernehmen gewählte Feature.
  - Verschieben und Drehen im Viewport (`MainWindow`): `SetPlacement`.
  - `SketchExtrusionToolController::accept`: `CreateProfileExtrusion` auf dem Stand, den
    das Werkzeug beim Start gespeichert hat.
  - `SketchEditorSession::tryAccept`: `EditSketch`, `CreateDatumSketch` oder die neue
    Aktion `CreateSketch` (Skizze auf einer Körperfläche mit ihrer Stützbindung), ebenfalls
    auf dem gespeicherten Stand. Der Skizzenentwurf bleibt lokaler Werkzeugzustand.
- **Fehlerbehandlung:** `ActionRequest::disposition` reicht die Wahl der Oberfläche
  (`keepEditableErrors` nach Rückfrage) durch. Ohne Angabe gilt der Standard des Befehls.
- **Hilfen:** `prepareRequest` (Request eines lokalen Aufrufers auf dem aktuellen oder
  einem gespeicherten Stand) und `src/desktop/ShellActions.hpp`.
- **Dispatcher:** Die Shell nutzt `executeAction` direkt. Lokale Requests werden nicht
  wiederholt und teilen sich eine ID; der Wiederholungsschutz des `ActionDispatcher` ist
  für Assistent und MCP gedacht.
- Keine sichtbare Änderung der Oberfläche, daher keine Aufnahmen. Diagnosen der
  Aktionsschicht erscheinen wie bisher über `CommandResult`.

## Prüfungen

| Test | Inhalt |
|---|---|
| `desktop.action_shell` | Über die echte Oberfläche: XY wählen, Rechteck mit Kreis skizzieren, Ring auf 8 extrudieren, Distanz in der Eigenschaftskarte auf 5, Box- und Zylinderbefehl. Dieselben Requests direkt auf einer zweiten Session ergeben denselben Dokumentinhalt (Features, Parameter, Tabelle), dieselben Featurezustände (alle aktuell) und dasselbe Volumen |
| bestehende Tests | `desktop.v01_flow`, `desktop.parameter_failures` (Fehlerrückfrage mit `keepEditableErrors`), `desktop.sketch_command_failure`, `application.sketch_editor_flow` und die übrigen Desktop-Tests laufen unverändert über die neuen Wege |

- **Windows:** 113/113 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 382 auf `b907002`, grün. Vorab auf Fedora mit GCC gebaut, 31/31 Tests der
  Anwendungsschicht sowie `action_shell`, `v01_flow`, `parameter_failures` und
  `sketch_command_failure` unter `kwin_wayland --virtual` bestanden.

## Grenzen

- Neu, Öffnen, Speichern, Undo/Redo und die Reparaturdialoge rufen die Session weiter
  direkt auf. Für sie gibt es noch keine Aktionen; Undo/Redo und Dateien sind keine
  Modellmutationen mit Ziel-ID.
- AI02e entfällt vorerst: Die Freigaben der Shell (`canAddCylinder`, `canTransform`)
  nutzen dieselben Session-Regeln wie die Fähigkeitsprüfung; Duplikate sind nicht
  entstanden.
