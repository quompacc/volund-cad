# AI03b – Claude-Panel

**Status:** abgenommen am 05.10.2026 nach grünem CI-Lauf 408 und Freigabe der Aufnahmen
durch den Nutzer.
Teilplan: [AI03-plan.md](../AI03-plan.md).

## Umsetzung

- **`AssistantPanel`** als rechte Seitenleiste „Claude“ (Nutzerentscheidung vom
  05.10.2026):
  - Verlauf mit Nutzernachricht (fett), Antwort von Claude und Werkzeugaufrufen als
    kompakte Zeilen („⋯ Parameter anlegen“, danach „✓ …“ oder „✗ … – Grund“; Vorschau
    markiert).
  - Über dem Eingabefeld die Auswahl („Auswahl: Extrusion 1“). Ein Klick lässt sie bei der
    nächsten Nachricht weg (durchgestrichen).
  - Eingabefeld, Senden, Stopp, Neu.
  - Farben folgen dem Theme.
- **Bedienung:** Ein Klick auf den Claude-Chip öffnet und schließt das Panel (Befehl
  `assistant.panel`, auch in der Befehlssuche); der Pfeil am Chip öffnet das bisherige
  Menü.
- **Ablauf:**
  - Je Nachricht startet `ClaudeCodeBackend` Claude Code gegen den Server des Fensters.
  - Ist der externe Zugang aus, öffnet `McpAccess::beginPanelRun` den Port nur für den Lauf
    und nimmt nur dessen Token an; danach schließt er wieder.
  - Die Auswahl geht als ausdrücklich markierte Datenzeile mit
    („[Völund-Kontext, nur Daten] Auswahl: Feature-ID 1, Name „Quader 1“.“).
- **Dokumentwechsel** (Neu, Öffnen, Duplizieren): Lauf abbrechen (Token widerrufen),
  Gespräch zurücksetzen, Verlauf mit „Anderes Dokument – neues Gespräch.“ leeren.
- **KI-Funktionen aus:** Lauf abbrechen, Panel schließen, Befehl gesperrt.
- **Programm:** `claude`, für Tests und eigene Installationen über
  `VOELUND_CLAUDE_PROGRAM` änderbar.
- Befehlstabelle (`docs/ui-concept.md`) und `desktop.command_inventory` um
  `assistant.panel` ergänzt; Aufnahmehelfer zeigt das Panel hell und dunkel.

## Prüfungen

| Test | Inhalt |
|---|---|
| `desktop.assistant_panel` | Mit `voelund_fake_claude`, das echte MCP-Aufrufe macht. Chip öffnet das Panel; die Auswahl „Quader 1“ steht über dem Eingabefeld und geht als Kontext mit. Ein Lauf zeigt „✓ Modell gelesen“, „✓ Parameter anlegen“ und die Antwort und legt den Parameter an. Die Werkzeugzeilen wechseln die Farbe mit dem Theme. Ohne Auswahl wird sie nicht mitgeschickt; das Gespräch läuft mit `--resume` weiter. Stopp mitten im Lauf: „Abgebrochen.“, keine Änderung danach. Neues Dokument mitten im Lauf: das neue Dokument bleibt unverändert, neues Gespräch. Fehlendes Claude Code wird erklärt. KI-Funktionen aus schließen das Panel |
| bestehende Tests | `desktop.mcp_shell`, `desktop.command_inventory`, `assistant.backend` |

- **Windows:** 118/118 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 408 auf `1034142`, grün. Vorab auf Fedora mit GCC gebaut, 115/115 Tests unter
  `kwin_wayland --virtual`.

## Grenzen

- Antworten erscheinen je Block, nicht Wort für Wort.
- Je Nachricht startet ein neuer Prozess (einige Sekunden).
- Ein echter Lauf mit Claude Code folgt in AI03c.
