# AI03 – Teilplan: Claude-Panel in Völund

Stand 05.10.2026. Grundlage: [Kapitel 11](11-ki-fem-optimierung.md), Abnahme A-AI-01 in
[08-abnahme.md](08-abnahme.md), [AI04-plan.md](AI04-plan.md) (MCP-Server),
[ADR 0012](../adr/0012-ui-shell-concept.md).

## Nutzerentscheidungen

**04./05.10.2026:**

- Kein API-Schlüssel in Völund. Das Panel startet **Claude Code** im Hintergrund mit dem
  Abo des Nutzers und verbindet es mit dem MCP-Server aus AI04.
- Ein Adapter mit API-Schlüssel (Claude, ChatGPT) bleibt für andere Nutzer vorgesehen und
  wird nicht jetzt gebaut.
- Erster Anwendungsfall mit vorhandenen Features (Parameter, Skizze, Extrusion, Box,
  Zylinder). Änderungen werden direkt ausgeführt und sind Undo-Schritte.

**05.10.2026 zur Bedienung:**

- **Rechtes Seitenpanel** „Claude“, ein- und ausgeblendet über den Claude-Chip. Später
  teilt es sich die rechte Seite per Reiter mit dem Parameter-Panel (PAR02).
- **Auswahl als Kontext, sichtbar:** Über dem Eingabefeld steht z. B. „Auswahl:
  Extrusion 1“. Die ID geht mit der Nachricht an Claude und lässt sich per Klick weglassen.
- **Nur Völund-Werkzeuge:** Alle eingebauten Werkzeuge von Claude Code sind aus (keine
  Dateien, keine Shell, kein Web); die Völund-Werkzeuge laufen ohne Rückfrage. Hooks und
  Einstellungen des Nutzers werden nicht geladen.
- **Ein Gespräch pro Dokument**, bis „Neu“ oder bis ein anderes Dokument geöffnet oder neu
  angelegt wird. Nach einem Neustart beginnt ein neues Gespräch.

## Architektur

- **Modul `src/assistant`** (Qt Core, keine Oberfläche):
  - `AssistantBackend` als austauschbare Schnittstelle: Nachricht senden, Ereignisse
    empfangen, abbrechen.
  - Erste Umsetzung `ClaudeCodeBackend`: je Nachricht ein Prozess
    ```
    claude -p <Nachricht> --output-format stream-json --verbose
           --tools "" --strict-mcp-config --mcp-config <Datei>
           --allowedTools mcp__volund__* --setting-sources ""
           [--resume <Sitzung>]
    ```
  - Die Ereignisse aus `stream-json` werden zu typisierten Ereignissen: Text, Beginn und
    Ende eines Werkzeugaufrufs mit Ergebnis, Abschluss, Fehler.
  - Die Sitzungs-ID aus dem ersten Lauf setzt das Gespräch mit `--resume` fort.
- **Ein Token je Lauf:**
  - Der MCP-Server bekommt zusätzlich zum Token für externe Clients kurzlebige Token für
    Panel-Läufe.
  - Abbruch, Dokumentwechsel und Ende des Laufs widerrufen das Token. Weitere Aufrufe dieses
    Laufs scheitern mit 401.
  - Weil der Server im GUI-Thread arbeitet, ist jeder Aufruf entweder vor dem Widerruf
    vollständig ausgeführt oder wird abgewiesen. So entsteht keine verspätete Änderung
    (A-AI-01).
- **Server bei ausgeschaltetem Zugang:** Ist der Claude-Zugang für externe Clients aus,
  öffnet das Panel den Port nur für die Dauer eines Laufs und nimmt dann nur das Token des
  Laufs an.
- **Kontext:** Jede Nachricht beginnt mit einer kurzen, als Daten markierten Zeile zur
  Auswahl (Name, ID). Namen und Kommentare aus dem Dokument sind Daten, keine Anweisungen
  (A-AI-01).
- **Ohne Claude Code:** Fehlt `claude`, oder ist der Nutzer nicht angemeldet, zeigt das
  Panel eine Anleitung. Völund läuft ohne Zugang normal weiter.

## Unterpakete

### AI03a – Backend und Ereignisse (ohne Oberfläche)

- `AssistantBackend`, `ClaudeCodeBackend`, Parser für `stream-json`, Widerruf des Tokens
  je Lauf im MCP-Server.
- **Test** mit einem Fake-`claude` (Testprogramm, das aufgezeichnete `stream-json`-Ausgabe
  abspielt und über HTTP echte Werkzeugaufrufe gegen den Server macht):
  - Argumente.
  - Ereignisfolge.
  - Fortsetzen mit `--resume`.
  - Abbruch mitten im Lauf: Danach scheitert jeder Aufruf mit 401, nichts ändert sich.
  - Fehlender Befehl und Exit-Code ungleich 0 als Fehler.
  - Unbekannte Ereignisse werden übergangen.

### AI03b – Panel

- Rechtes Seitenpanel mit:
  - Verlauf: Nachrichten des Nutzers, Antworten von Claude, Werkzeugaufrufe als kompakte
    Zeilen („✓ Skizze 1, Extrusion 1 angelegt“, „✗ abgewiesen: staleStamp“).
  - Auswahlzeile, Eingabefeld, Senden, Stopp, „Neu“.
- Der Chip öffnet und schließt das Panel; sein Menü bleibt.
- Dokumentwechsel beendet den Lauf und das Gespräch.
- **Vorher Aufnahmen vorlegen.**
- **Test:** Desktop-Test mit Fake-`claude`: Ablauf, Stopp, Dokumentwechsel während des
  Laufs, Auswahlkontext, fehlendes Claude Code.

### AI03c – Abnahme A-AI-01

- Abnahmehelfer mit echtem Claude Code unter Windows, wie in AI04d:
  - Referenzplatte auswählen, Bohrungsmaß über das Panel ändern, eine mit vorhandenen
    Features darstellbare Rippe ergänzen lassen.
  - Mehrdeutiger Bezug führt zur Rückfrage.
  - Undo/Redo, Speichern und Öffnen.
- Feste Fälle mit Fake-`claude`: Abbruch, Dokumentwechsel, Providerfehler, Name mit
  vermeintlicher Anweisung.

## Bewusst nicht in AI03

- Adapter mit API-Schlüssel, ChatGPT, Claude Desktop.
- Bestätigung vor jeder Änderung (spätere Einstellung).
- Bilder des Viewports als Kontext.
- Gespeicherter Verlauf je Datei.
- Claude Code auf Fedora (Nutzerentscheidung); Linux über Tests mit Fake-`claude`.
