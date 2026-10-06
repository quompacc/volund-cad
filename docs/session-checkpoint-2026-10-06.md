# Fortsetzung – 6. Oktober 2026

**Einstieg für die nächste Sitzung:** Dieses Dokument zuerst lesen. Arbeitsregeln,
Werkzeuge und Zugänge stehen unverändert in
[session-checkpoint-2026-10-04.md](session-checkpoint-2026-10-04.md) und gelten weiter.

Auftrag des Nutzers (06.10.2026), **nacheinander** abzuarbeiten:

1. Die von Claude in AI03c benannten Werkzeuglücken (Abschnitt „Block A“).
2. Danach die geplanten Pakete U07–U10 (Abschnitt „Block B“).

## Stand des Repos

- Branch `ux01`, sauber. `origin/main` steht auf `197553c` (AI03 abgenommen, CI 414 grün).
- Auf `ux01` liegt darüber nur `4700973` (Skizzen-Rückmeldungen, siehe unten).
  **CI-Lauf 416** lief bei Sitzungsende noch. Zuerst prüfen; bei Erfolg
  `git push origin 4700973:main` (Fast-Forward), bei Fehler zuerst beheben.
- Windows: 120/120 Tests. Fedora (GCC, `kwin_wayland --virtual`): 117/117.

## Seit dem letzten Checkpoint erledigt (05./06.10.2026)

Alles auf Nutzerwunsch, außerhalb der Paketliste, ohne eigene Paket-ID:

| Commit | Inhalt |
|---|---|
| `7f0ed1e` | Klick ins Leere wählt Ebene, Fläche und Feature ab |
| `ad81573` | Symbole für die Skizzenwerkzeuge (Name nur im Tooltip) |
| `796db57` | ViewCube als echter 3D-Würfel, dreht mit der Kamera; Flächen, Kanten, Ecken, Ziehen, Rollen, Haus |
| `4700973` | Hilfsgeometrie gestrichelt; Klick in die Fläche einer Skizze wählt das Profil zum Extrudieren; Bestimmtheit sichtbar (blau = frei, dunkel = bestimmt, rot = Widerspruch) mit Statuszeile „Freiheitsgrade: N“ / „✓ Vollständig bestimmt“; Klick ins Leere hebt die Auswahl im Skizzeneditor auf |

- Der Nutzer hat Symbole und ViewCube freigegeben („sieht gut aus“). Die Aufnahmen
  der Skizzen-Rückmeldung (`voelund_mcp_chip_capture`, Bild `9-sketch-state`) hat er
  gesehen; er testet gerade selbst in Völund.
- Neue Tests: `desktop.sketch_feedback`, `desktop.view_cube`, Solver-Test
  `adapterReportsFreeElements`.
- Technik: `SketchSolveResult::freeElements` (über `GCS::System::getDependentParams`
  nach `diagnose()`), `CadSession::selectSketchProfileAt`,
  `CadViewport::setAreaClickHandler`, `ToolPanelHost::setStatus`.

## Block A – Werkzeuglücken aus AI03c

Claude hat in der Abnahme A-AI-01 bemerkt, dass es eine Rippe auf einer Platte nicht
sauber bauen kann. Ziel: Diese Fälle sind in der Oberfläche **und** über MCP (Aktionsschicht
`Actions.hpp` → `ToolCatalog.cpp`) möglich.

Vorhandene MCP-Werkzeuge (16): `get_context`, `create_box`, `create_cylinder`,
`set_box_dimensions`, `set_cylinder_size`, `set_placement`, `create_sketch`,
`edit_sketch`, `create_sketch_extrusion`, `create_profile_extrusion`,
`set_extrusion_distance`, `bind_extrusion_distance`, `bind_sketch_dimension`,
`create_parameter`, `rename_parameter`, `set_parameter_value`.

Vorgeschlagene Reihenfolge (jeweils eigenes Paket mit Plan, z. B. `T01`–`T04`; IDs
und Zuschnitt in der neuen Sitzung festlegen und in `packets.json` aufnehmen):

1. **Körper vereinigen / Extrusion verbindet mit dem Körper.** Heute erzeugt jede
   Extrusion einen eigenen Körper. Gebraucht wird eine Operation (Neuer Körper /
   Vereinigen / Abziehen) an der Extrusion. `BRepAlgoAPI_Fuse`/`Cut`/`Common` liegen
   bereits in `src/kernel/ShapeOperations.cpp`. Schemaänderung im Dokument nötig
   (Version anheben, Migration, Tests für Lesen alter Dateien).
2. **Skizze auf einer Körperfläche über MCP.** In der Oberfläche gibt es Skizzen auf
   Flächen bereits über die Flächenauswahl; über MCP fehlt eine stabile Flächenreferenz.
   Vorgaben aus [subshape-reference-contract.md](subshape-reference-contract.md)
   beachten (keine flüchtigen Indizes nach außen geben).
3. **Extrusion mit Versatz / von einer Fläche aus.** Startversatz entlang der Normalen
   und ggf. „bis Fläche“. Ebenfalls Schemaänderung.
4. **Verschieben von Skizzen- bzw. Extrusionskörpern** (in AI03c als
   `unsupportedTarget` abgewiesen). Optional, mit dem Nutzer klären.

Vor dem Start mit dem Nutzer abstimmen (ADR 0012): sichtbare Bedienung im Werkzeugpanel
der Extrusion (Operation, Versatz) als Skizze/Aufnahme vorlegen. Nach Block A die
Abnahme mit echtem Client (`voelund_assistant_panel_acceptance`, nur Windows) um den
Rippenfall erweitern: Rippe sitzt jetzt auf der Platte und ist mit ihr ein Körper.

## Block B – Pakete U07–U10

Alle Abhängigkeiten sind erledigt (U05, U06, D03, D06, D07, D14, R06). Details stehen in
[packets.json](implementation-plan/packets.json) und
[Kapitel 4](implementation-plan/04-oberflaeche-skizze.md).

| Paket | Inhalt | Abnahme |
|---|---|---|
| U07 | New/Open/Save und Close-Schutz in der Shell | A-IO-01, A-UI-03 |
| U08 | Skizzen-Erzeugung auf einen Weg vereinheitlichen | gleiche Editorzustände aus beiden Einstiegen |
| U09 | Input und Drag als eine Benutzeraktion | A-TXN-03 (100 Moves → ein Undo) |
| U10 | Fehler und reparierbare Referenzen sichtbar machen | A-ERR-01, A-REF-03 |

Hinweise:

- U07: Teile (Save/Discard/Cancel) könnten durch die Shell-Arbeit schon existieren;
  zuerst Bestand prüfen und nur die Lücken schließen.
- U08/U09: `apps/voelund-ui-preview` ist seit U12a entfernt. Die Dateilisten in
  `packets.json` nennen es noch; diese Teile entfallen, nicht wiederbeleben.
- U10 hat sichtbare UI (Fehleranzeige, Reparatur): vorher mit dem Nutzer abstimmen.

## Offene Punkte (unverändert)

- Spiegelzellen per RegionId (Nutzerentscheidung nötig).
- PAR02 (Parameter-UI) steht aus.
- GitHub-Spiegel nicht aktuell; nur auf Wunsch synchronisieren.
