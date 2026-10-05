# AI04c – Anbindung an die Shell

**Status:** Teil 1 (Server im laufenden Programm, ohne sichtbare Bedienung) umgesetzt, CI-Lauf
ausstehend. Teil 2 (sichtbare Bedienung) wird vorher mit dem Nutzer abgestimmt (ADR 0012).
Teilplan: [AI04-plan.md](../AI04-plan.md).

## Umsetzung (Teil 1)

- **`McpAccess`** (`include/voelund/desktop/McpAccess.hpp`): `ToolHost` und
  `McpHttpServer` eines laufenden Völund. Nach jeder veröffentlichten Änderung ruft er
  `MainWindow::refreshAfterExternalChange()` auf.
- **Einstellungen:** Port (Standard 47913) und Token stehen in den Benutzereinstellungen
  (`mcp/port`, `mcp/token`). Ein fehlender Token wird erzeugt (32 Zufallsbytes, hex). Damit
  bleibt ein einmal eingerichteter Client gültig.
- **Start:** vorerst mit `voelund-cad --mcp`. Der Befehl für Claude Code
  (`claude mcp add --transport http volund http://127.0.0.1:47913/mcp --header
  "Authorization: Bearer …"`) steht im Log und in `mcp-connect.txt` im
  Konfigurationsverzeichnis.
- **`MainWindow::refreshAfterExternalChange()`:** aktualisiert Ansicht, Modellbaum,
  Eigenschaften, Status und Befehle. Anders als nach einem eigenen Befehl bleiben offene
  Werkzeuge offen.
- **Parallele Änderung im Skizzeneditor:** Der Editor merkt sich beim Bearbeiten einer
  vorhandenen Skizze deren gespeicherten Inhalt. Hat sich die Skizze inzwischen geändert
  (z. B. über MCP), wird das Übernehmen abgewiesen; der Entwurf bleibt offen, nichts wird
  überschrieben. Änderungen an anderen Features stören nicht. Skizzen auf Körperflächen
  prüfen wie bisher zusätzlich den Stand der Stütze.
- **Windows:** `Qt6Network.dll` wird mit den anderen Qt-DLLs neben Programm und Tests
  kopiert.

## Prüfungen

| Test | Inhalt |
|---|---|
| `desktop.mcp_shell` | Echtes Fenster mit MCP-Server auf freiem Port und HTTP-Client. Der Nutzer zeichnet auf XY ein Rechteck; währenddessen legt Claude eine Box an. Die Box erscheint im Modellbaum, der Skizzenentwurf bleibt offen und wird danach neben der Box angelegt. Der Nutzer öffnet die Skizze zum Bearbeiten, Claude ergänzt darin über `edit_sketch` einen Kreis. Das Übernehmen des älteren Entwurfs wird abgewiesen, Claudes Stand bleibt, der Editor bleibt offen. Undo nimmt Claudes Änderung zurück |
| bestehende Tests | Skizzeneditor, Stützbindung, Solver-Freigabe und ID-Kapazität mit Editoren ohne Skizze in der Session |

- **Windows:** 116/116 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf ausstehend. Vorab auf Fedora mit GCC gebaut, 113/113 Tests unter
  `kwin_wayland --virtual` bestanden.

## Vorab-Durchlauf mit echtem Client

Claude Code 2.1.289 unter Windows, headless über `--mcp-config` mit diesem Server (nur
`mcp__volund__*` erlaubt). Auftrag: Parameter A = 8 mm, Platte 80 × 40 × 10 zentriert auf
XY mit Bohrung, Radius an A gebunden, danach A = 10 mm. Claude Code hat dafür
`get_context`, `create_parameter`, `create_sketch_extrusion`, `set_parameter_value` und
erneut `get_context` aufgerufen und die Fläche selbst nachgerechnet (3200 − 100π mm²).
Der erste Versuch von `create_sketch_extrusion` wurde abgewiesen, ohne dass sich etwas
änderte: Werte an `coordinateX`/`coordinateY` lehnte der Codec fälschlich ab. Das ist
in `mcp.tools` behoben (Koordinaten tragen einen Wert, sind aber nicht bindbar) und
geprüft. Die förmliche Abnahme mit echtem Client folgt in AI04d.

## Grenzen

- Noch keine sichtbare Bedienung (Ein/Aus, Port, Token, Befehl zum Kopieren); Start nur
  über `--mcp`.
- Eine über MCP angelegte Box oder Skizze wird wie bei einem eigenen Befehl ausgewählt.
