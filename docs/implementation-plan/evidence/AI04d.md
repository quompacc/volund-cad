# AI04d – Abnahme mit echtem Client

**Status:** abgenommen am 05.10.2026 (Windows, echter Client) nach grünem CI-Lauf 401;
damit ist AI04 abgeschlossen. Teilplan:
[AI04-plan.md](../AI04-plan.md).

Nutzerentscheidung vom 05.10.2026: Die Abnahme mit echtem Client erfolgt nur unter
Windows. Auf dem Fedora-Rechner wird vorerst kein Claude Code installiert; Linux ist über
die CI-Tests `mcp.tools`, `mcp.http` und `desktop.mcp_shell` abgedeckt.

## Ablauf

Abnahmehelfer `voelund_mcp_client_acceptance` (`EXCLUDE_FROM_ALL`, kein CI-Test, weil er
ein angemeldetes Claude Code braucht):

- Er öffnet ein echtes Völund-Fenster mit eingeschaltetem Claude-Zugang auf einem freien
  Port.
- Er startet Claude Code 2.1.289 zweimal headless als eigenen Prozess (`claude -p`, nur
  dieser Server, nur `mcp__volund__*` erlaubt). Jeder Lauf ist eine neue Verbindung und
  eine neue MCP-Sitzung.
- Die Prüfungen laufen gegen die Session des Fensters.

| Schritt | Ergebnis |
|---|---|
| Lauf 1: Parameter A = 8 mm, Platte 80 × 40 × 10 auf XY, zentriert, Bohrung in der Mitte mit Radius an A gebunden. Nach Claudes erster Änderung legt der „Nutzer“ im Fenster einen Quader an | A = 8 mm; Volumen = Platte mit Bohrung r = 8 plus Quader (45 989,38 mm³); der Quader des Nutzers ist unverändert. Claude (9 Runden) nennt ihn in seiner Antwort und hat ihn nicht angefasst |
| Lauf 2 (neuer Prozess, neue Sitzung): A = 10 mm, Dicke 12 mm, sonst nichts | A = 10 mm; Volumen = Platte 12 mm mit Bohrung r = 10 plus Quader (50 630,09 mm³); Claude (5 Runden) |
| Undo im Fenster | Zwei Undo-Schritte nehmen Claudes zwei Änderungen aus Lauf 2 zurück und ergeben wieder den Stand nach Lauf 1 |

Ein früherer Lauf (AI04c) zeigte, dass abgewiesene Aufrufe nichts ändern und Claude Code
danach mit einem korrigierten Aufruf fortfährt.

## Abnahme A-ACTION-01 über den Transport

| Punkt | Nachweis |
|---|---|
| Gleicher Request über MCP und direkt ergibt gleiche Ergebnisse und Fehler | `mcp.tools` (AI04a) |
| Reconnect und Wiederholung ohne doppelte Änderung | `mcp.http` (AI04b); Lauf 2 über eine neue Sitzung |
| Fremdes Dokument, alter Stand, fehlendes Ziel, falsche Einheit über MCP | `mcp.tools`, `mcp.http`; NaN ist in JSON nicht darstellbar |
| Parallele Nutzeränderung | `desktop.mcp_shell` (AI04c) und Lauf 1 |
| Tatsächlicher Client | dieser Nachweis |

## Grenzen

- Ein echter Client nur unter Windows; Fedora nach Nutzerentscheidung ohne Claude Code.
- Antworten eines Sprachmodells sind nicht deterministisch. Der Helfer prüft deshalb das
  Ergebnis im Dokument, nicht die Folge der Werkzeugaufrufe.
- Eine verlorene Antwort mit automatischer Wiederholung durch den echten Client lässt sich
  nicht gezielt auslösen; die Wiederholung ist über `mcp.http` nachgewiesen.
