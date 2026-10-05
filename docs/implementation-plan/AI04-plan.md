# AI04 – Teilplan: MCP-Zugang für Claude Code

**Abgeschlossen am 05.10.2026:** [AI04a](evidence/AI04a.md), [AI04b](evidence/AI04b.md),
[AI04c](evidence/AI04c.md), [AI04d](evidence/AI04d.md).

Stand 05.10.2026. Grundlage: [Kapitel 11](11-ki-fem-optimierung.md) (Aktionsvertrag AI01),
[AI02-plan.md](AI02-plan.md) (Aktionsschicht), Abnahme A-ACTION-01 in
[08-abnahme.md](08-abnahme.md).

## Nutzerentscheidung vom 05.10.2026

- **AI04 kommt vor AI03.** Völund speichert keinen API-Schlüssel. Stattdessen stellt Völund
  einen lokalen MCP-Server bereit, und der Client des Nutzers verbindet sich damit. Ein
  Claude-Abo darf nur in Anthropics eigenen Programmen genutzt werden, nicht in einer
  fremden App.
- **Erster Client ist Claude Code.** Er ist weiter verbreitet und läuft auch unter Linux.
  Claude Desktop (über eine `stdio`-Brücke) folgt bei Bedarf.
- **Änderungen werden direkt ausgeführt** und sind normale Undo-Schritte. Eine Bestätigung
  in Völund vor jeder Änderung kommt später als Einstellung.
- **Erster Anwendungsfall mit vorhandenen Features:** Parameter, Skizze auf einer
  Basisebene, Profil-Extrusion, Box und Zylinder. Bohrung und Rippe als eigene Features
  sind nicht nötig.
- **AI03** (Seitenpanel in Völund) folgt danach und kann Claude Code im Hintergrund mit
  diesem Server starten. Ein austauschbarer Adapter mit API-Schlüssel bleibt für andere
  Nutzer vorgesehen.

## Architektur

- **Modul `src/mcp`** (Bibliothek `Voelund::Mcp`): JSON-Codec, MCP-Protokoll
  (JSON-RPC 2.0) und HTTP-Transport. Abhängig nur von Qt Core, Qt Network (beide in
  qtbase, unter Windows und Fedora vorhanden) und `Voelund::Application`. Qt HttpServer
  fehlt in der Windows-Installation; das schlanke HTTP entsteht deshalb auf `QTcpServer`.
- **Ausführung:** Der Server läuft im Desktop-Prozess auf dem GUI-Thread. Jede Mutation
  wird damit auf dem Eigentümerthread der Session veröffentlicht (AI01), ohne Worker und
  ohne zweiten Featurebaum. Ein `ActionDispatcher` je Sitzung des Programms: Der
  Wiederholungsschutz gilt auch über einen Reconnect des Clients.
- **Transport:** MCP „Streamable HTTP“ mit einfachen JSON-Antworten auf `POST /mcp`, ohne
  Server-Push (kein SSE; `GET` liefert 405).
- **Sicherheit:**
  - Bindung nur an `127.0.0.1`.
  - Zufälliger Bearer-Token, den Claude Code als Header mitsendet.
  - Prüfung des `Origin`-Headers gegen DNS-Rebinding aus dem Browser.
  - Größenlimit je Anfrage.
  - Namen, Kommentare und Dateiinhalte sind Daten und ändern keine Regeln (AI01).

## Werkzeuge (Version 1)

- `get_context`: Stand, Einheiten, Features, Gliederung, Zustände, Parameter, Regionen
  je Skizze, verfügbare Aktionen je Ziel (`describeActionContext`).
- Ein Werkzeug je Aktion, z. B. `create_parameter`, `set_parameter_value`,
  `bind_sketch_dimension`, `create_sketch_extrusion`, `create_profile_extrusion`,
  `set_extrusion_distance`, `create_box`, `set_placement`.
  - Gemeinsame Felder: `requestId`, `documentId`, `stamp` (Generation, Revision),
    `preview` (optional; dann `previewAction` statt Ausführung).
  - Zahlen mit Einheit als `{"value": 10, "unit": "mm"}`; das Schema lässt nur die
    erlaubten Einheiten zu.
  - Skizzen nur auf Basisebenen (`xy`, `xz`, `yz`). Elemente: Linie, Rechteck, Kreis,
    Bogen. Bedingungen: zunächst die Maße (Abstand, Radius, Durchmesser, Winkel …), auf
    Wunsch an Parameter gebunden.
- Das Ergebnis enthält Status, Ablehnungsgrund, Diagnose, neuen Stand, erzeugte und
  geänderte IDs und `repeated`, als strukturierter Inhalt und als kurzer Text.
- Die Werkzeugbeschreibungen sind Teil der Bedienung: Sie erklären Stand, Einheiten und
  die Wiederholung mit derselben `requestId`.

## Unterpakete

Reihenfolge verbindlich. Jedes Unterpaket hat einen eigenen Nachweis, einen grünen
CI-Lauf und einen lokalen Windows-Lauf.

### AI04a – JSON-Codec und Werkzeugkatalog (ohne Netz)

- Kontext und Ergebnis als JSON; JSON zu `ActionRequest` mit Schemaprüfung.
- Werkzeugkatalog mit Name, Beschreibung und `inputSchema`.
- Skizzen: Prüfen, ob gelieferte Geometrie vor dem Anlegen gelöst wird
  (`solveSketchConstraints`), damit der Client ungefähre Koordinaten plus Maße liefern
  kann. Ein unlösbarer Entwurf wird ohne Mutation abgewiesen.
- **Test:** Rundlauf jeder Aktion; fehlende Felder, falsche Typen und Einheiten ergeben
  strukturierte Fehler ohne Mutation.

### AI04b – MCP-Protokoll und HTTP-Transport

- `initialize` (Protokollversion aushandeln), `notifications/initialized`, `ping`,
  `tools/list`, `tools/call`; JSON-RPC-Fehler für unbekannte Methoden und kaputtes JSON.
- Sitzungs-ID (`Mcp-Session-Id`), Token, Origin, Größenlimit, nur `127.0.0.1`.
- **Test** mit einem Client im selben Prozess (`QTcpSocket`): Handshake, Aufruf,
  Reconnect und Wiederholung derselben `requestId` ohne zweite Mutation, falscher Token,
  fremder Origin.

### AI04c – Anbindung an die Shell

- Server im Desktop starten und beenden; nach jeder Mutation die Szene aktualisieren wie
  bei einem Shell-Befehl.
- Sichtbare Bedienung (Ein/Aus, Port, Token, Befehl zum Kopieren für `claude mcp add`):
  **vor der Umsetzung mit dem Nutzer abstimmen**, Aufnahmen vorlegen (ADR 0012).

### AI04d – Abnahme mit echtem Client

- Claude Code verbindet sich über
  `claude mcp add --transport http volund http://127.0.0.1:<Port>/mcp --header "Authorization: Bearer <Token>"`.
- Ablauf: Referenzplatte mit Bohrung über Parameter anlegen und eine Abmessung ändern.
  Unter Windows und auf Fedora. Der Nachweis beschreibt den Ablauf kurz, ohne Logs.

## Abnahme A-ACTION-01 über den Transport

| Punkt | Unterpaket |
|---|---|
| Gleicher Request ergibt über MCP und direkt gleiche Ergebnisse und Fehler | AI04a/b |
| Reconnect und Wiederholung erzeugen keine doppelte Änderung | AI04b |
| Fremdes Dokument, alter Stand, fehlendes Ziel, NaN, falsche Einheit über MCP | AI04a/b |
| Tatsächlicher Client | AI04d |

## Bewusst nicht in AI04

- Chat in Völund (AI03), Claude Desktop, entfernter Zugriff über das Netz.
- Skizzen auf Körperflächen über MCP (Flächenbezug über `SubshapeRef` fehlt noch).
- Datei öffnen und speichern über MCP; Undo/Redo als Werkzeuge erst bei Bedarf.

## Offene Punkte

- Fester oder zufälliger Port; Ablage des Tokens (Einstellungen, neu erzeugbar).
- Mehrere offene Völund-Instanzen.
