# AI04c – Anbindung an die Shell

**Status:** Teil 1 (Server im laufenden Programm) und Teil 2 (sichtbare Bedienung nach
Nutzerentscheidung vom 05.10.2026) umgesetzt. Die Aufnahmen liegen dem Nutzer zur Freigabe
vor; CI-Lauf ausstehend. Teilplan: [AI04-plan.md](../AI04-plan.md).

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

## Umsetzung (Teil 2, sichtbare Bedienung)

Nutzerentscheidung vom 05.10.2026: Chip in der Kopfzeile, letzter Zustand wird gemerkt,
und jede Änderung von Claude wird dreifach angezeigt.

- **Chip „Claude: aus / bereit / verbunden“** rechts neben dem Hell/Dunkel-Schalter.
  „verbunden“ heißt: Ein Client hat sich in den letzten fünf Minuten angemeldet.
- **Menü am Chip:**
  - `assistant.mcp`: Zugang ein/aus.
  - Die Adresse.
  - `assistant.mcp_copy_command`: Befehl für `claude mcp add` in die Zwischenablage.
  - `assistant.mcp_new_token`: neuer Token; bestehende Clients müssen neu verbunden werden.
  - Der Hinweis, dass jede Änderung ein Rückgängig-Schritt ist.
  - Statt einer schwebenden Karte ein Aufklappmenü wie das Dateimenü: Die schwebende Karte
    am Viewport gehört nach U05 Werkzeugpanel und Eigenschaften.
- **Start:** Der Zustand Ein/Aus steht in `mcp/enabled`. Standard ist aus; wer den Zugang
  einschaltet, hat ihn beim nächsten Start wieder. `--mcp` schaltet ihn ebenfalls ein.
- **Rückmeldung je Änderung von Claude:**
  - Statuszeile, z. B. „Claude: Skizze 1, Extrusion 1 angelegt – Strg+Z nimmt es zurück“.
  - Der Chip leuchtet kurz in der Akzentfarbe auf.
  - Die erzeugten oder geänderten Features stehen 1,5 s fett in Akzentfarbe im
    Modellbaum, auch wenn sich der Baum dazwischen neu aufbaut.
- **Befehlstabelle** in `docs/ui-concept.md` und `desktop.command_inventory` um die drei
  Befehle ergänzt.
- **Aufnahmehelfer** `voelund_mcp_chip_capture` (`EXCLUDE_FROM_ALL`, kein Test): Chip aus,
  Menü, Änderung durch Claude, dunkle Darstellung.

## Prüfungen

| Test | Inhalt |
|---|---|
| `desktop.mcp_shell` | Gemerkter Zustand „an“ startet den Server, der Chip zeigt „bereit“; der kopierte Befehl enthält Adresse und Token; nach der Anmeldung des Clients „verbunden“. Echtes Fenster mit MCP-Server auf freiem Port und HTTP-Client. Der Nutzer zeichnet auf XY ein Rechteck; währenddessen legt Claude eine Box an. Die Box erscheint im Modellbaum, der Skizzenentwurf bleibt offen und wird danach neben der Box angelegt. Der Nutzer öffnet die Skizze zum Bearbeiten, Claude ergänzt darin über `edit_sketch` einen Kreis. Das Übernehmen des älteren Entwurfs wird abgewiesen, Claudes Stand bleibt, der Editor bleibt offen. Statuszeile, Aufleuchten des Chips und Markierung im Modellbaum bei Claudes Box. Undo nimmt Claudes Änderung zurück. Ausschalten schließt den Port, der Chip zeigt „aus“, der Zustand ist gespeichert |
| `desktop.command_inventory` | Die drei neuen Befehle stehen in der Tabelle; im leeren Dokument sind Ein/Aus und neuer Token aktiv, Kopieren nur bei laufendem Zugang |
| bestehende Tests | Skizzeneditor, Stützbindung, Solver-Freigabe und ID-Kapazität mit Editoren ohne Skizze in der Session |

- **Windows:** 116/116 Tests bestanden (Debug, MSVC, seriell), nach Teil 1 und Teil 2.
- **Linux:** CI-Lauf ausstehend. Vorab auf Fedora mit GCC gebaut, 113/113 Tests unter
  `kwin_wayland --virtual` bestanden, nach Teil 1 und Teil 2.

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

- Der Port lässt sich nur über die Einstellung `mcp/port` ändern.
- Gefunden nebenbei, nicht Teil von AI04: In der dunklen Darstellung sind die Einträge
  „Ursprung“ im Modellbaum nach einem Themenwechsel unsichtbar; ihre Farbe folgt dem
  Wechsel nicht.
- Eine über MCP angelegte Box oder Skizze wird wie bei einem eigenen Befehl ausgewählt.
