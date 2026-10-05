# AI03a – Backend und Ereignisse

**Status:** abgenommen am 05.10.2026 nach grünem CI-Lauf 407. Teilplan: [AI03-plan.md](../AI03-plan.md).

## Umsetzung

- **Modul `src/assistant`** (`Voelund::Assistant`, nur Qt Core):
  - `AssistantBackend`: austauschbare Schnittstelle mit Senden, Abbrechen, Zurücksetzen
    und typisierten Ereignissen (Start, Text, Werkzeugaufruf begonnen/beendet mit
    strukturiertem Ergebnis, Ende, Fehler, Abbruch).
  - `parseStreamJsonLine` übersetzt die Ausgabe von `claude -p --output-format
    stream-json`. Unbekannte Zeilen und Ereignisarten werden übergangen. Meldet `init`,
    dass der Völund-Server nicht verbunden ist, ist das ein Fehler.
- **`ClaudeCodeBackend`:**
  - Je Nachricht ein Prozess mit
    `-p … --output-format stream-json --verbose --tools "" --strict-mcp-config
    --mcp-config <Datei> --allowedTools mcp__volund__* --setting-sources ""`, ab der
    zweiten Nachricht mit `--resume <Sitzung>`.
  - Umgebung `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1`.
  - Die MCP-Konfiguration mit dem Token des Laufs liegt in einem temporären Verzeichnis.
  - Fehler: fehlendes Programm („nicht gefunden“ mit Hinweis auf die Installation),
    Exit-Code ungleich 0 mit letzter Fehlerzeile, Absturz, Völund nicht erreichbar (der
    Prozess wird dann sofort beendet).
- **Token je Lauf im MCP-Server:** `issueRunToken` und `revokeRunToken` neben dem Token
  für externe Clients, das sich mit `setMainTokenAccepted(false)` abschalten lässt. Der
  Lauf widerruft sein Token beim Ende; ein Abbruch widerruft es, bevor der Prozess beendet
  wird. Ohne Token (KI-Funktionen aus) startet kein Lauf.
- **Datenschutzschalter** (eigener Commit `5355ece`, Nutzerwunsch): „KI-Funktionen
  erlauben“, `VOELUND_DISABLE_AI`; siehe Teilplan.

## Prüfungen

| Test | Inhalt |
|---|---|
| `assistant.backend` | Parser (Text, Werkzeugaufruf, Ergebnis mit Struktur, Fehler-Ergebnis, Unbekanntes). Gegen `voelund_fake_claude`, das echte MCP-Aufrufe macht und `stream-json` ausgibt; der Server nimmt dabei nur Token je Lauf an. Ein Lauf legt einen Parameter an: Ereignisfolge, Sitzung, Token danach widerrufen. Aufgezeichnete Argumente: keine eingebauten Werkzeuge, nur `mcp__volund__*`, keine Nutzereinstellungen, `stream-json`, Nebenverkehr aus. Zweiter Lauf mit `--resume`, unbekannte Ausgabe übergangen. Widerrufenes Token mitten im Lauf: Die spätere Änderung wird abgewiesen, das Dokument bleibt unverändert. Stopp: sofort „abgebrochen“, Token widerrufen, keine Änderung, keine weiteren Ereignisse. Nicht erreichbarer Server beendet den Lauf sofort. Exit-Code 3 mit Fehlermeldung. Fehlendes Programm. KI-Funktionen aus: kein Start |

- **Windows:** 117/117 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 407 auf `3c07add`, grün. Vorab auf Fedora mit GCC gebaut: 34/34 Tests der
  Anwendungsschicht, von MCP und Assistent.

## Grenzen

- Noch keine Oberfläche (AI03b). Das Format von `stream-json` ist mit einem
  aufgezeichneten Lauf von Claude Code 2.1.289 abgeglichen (Start, Text, Ende); Lauf mit
  Werkzeugaufrufen des echten Clients folgt in AI03c.
- Je Nachricht ein neuer Prozess: Der Start kostet einige Sekunden. Ein dauerhafter
  Prozess mit `--input-format stream-json` ist möglich, wenn das stört.
