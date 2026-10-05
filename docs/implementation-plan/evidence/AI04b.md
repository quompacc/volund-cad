# AI04b – MCP-Protokoll und HTTP-Transport

**Status:** abgenommen am 05.10.2026 nach grünem CI-Lauf 391. Teilplan: [AI04-plan.md](../AI04-plan.md).

## Umsetzung

- **`McpProtocol`** (JSON-RPC 2.0, unabhängig vom Transport):
  - `initialize` handelt die Protokollversion aus (2025-11-25, 2025-06-18, 2025-03-26;
    sonst die neueste), meldet die Fähigkeit `tools` und gibt dem Client kurze
    Anweisungen mit (zuerst `get_context`, Einheiten, eigene `requestId` je Änderung,
    Wiederholung nach verlorener Antwort, neuer Kontext nach `staleStamp`).
  - `ping`, `tools/list`, `tools/call`. Benachrichtigungen und Antworten des Clients
    bleiben ohne Antwort.
  - Fehler: unbekannte Methode (-32601), unbekanntes Werkzeug oder Argumente kein Objekt
    (-32602), unerwartete Ausnahme (-32603), kein JSON-RPC 2.0 (-32600).
- **`McpHttpServer`** auf `QTcpServer` (Qt Network, Teil von qtbase; kein Qt HttpServer):
  - Nur `127.0.0.1`, freier oder fester Port. `url()` liefert die Adresse für
    `claude mcp add`.
  - HTTP/1.1 mit `Content-Length`, mehrere Anfragen je Verbindung; `POST /mcp` mit
    JSON-Antwort, Benachrichtigungen mit 202, `GET` mit 405 (kein Server-Push), `DELETE`
    beendet eine Sitzung.
  - Sitzungen über `Mcp-Session-Id`. Ohne Sitzung 400, mit unbekannter 404; der Client
    initialisiert dann neu. Der `ActionDispatcher` gehört zum `ToolHost`, nicht zur
    Sitzung: Eine Wiederholung nach einem Reconnect ändert nichts doppelt.
  - Schutz:
    - Bearer-Token mit Vergleich in konstanter Zeit (401).
    - `Host` muss `127.0.0.1:<Port>` oder `localhost:<Port>` sein, ein mitgesendeter
      `Origin` ebenso (403, gegen DNS-Rebinding aus dem Browser).
    - Höchstens 1 MiB je Anfrage (413, schon am Header erkannt) und 16 KiB Header (431).
    - `Transfer-Encoding` wird nicht unterstützt (501).
    - Unbekannte `MCP-Protocol-Version` (400), nur `application/json` (415), keine
      Batches (400), kaputtes JSON (-32700).
- Der Server läuft im Thread der Session; die Anbindung an die Shell folgt in AI04c.
- Windows: Die Tests bekommen `Qt6Network.dll` per Post-Build-Kopie wie die übrigen
  Qt-DLLs.

## Prüfungen

| Test | Inhalt |
|---|---|
| `mcp.http` | Echter Socket-Client im selben Prozess: `initialize` mit Sitzung und Version, `notifications/initialized` (202), `tools/list` (16 Werkzeuge), `get_context`, `create_parameter` ändert das Dokument. Neue Verbindung und neue Sitzung wiederholen dieselbe Anfrage: `repeated`, keine zweite Änderung. Falscher Token, fremder Host, fremder Origin, unbekannte Sitzung, `GET`, kaputtes JSON, Batch, unbekannte Methode, unbekanntes Werkzeug, übergroße Anfrage, beendete Sitzung: jeweils abgewiesen, Stand und Änderungszähler unverändert |

- **Windows:** 114/115 im Gesamtlauf; `platform.atomic_file_store` scheiterte einmal nach
  11 s und bestand danach 5/5 Wiederholungen. Der Test berührt MCP nicht. Alle übrigen
  Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 391 auf `cb08676`, grün. Vorab auf Fedora mit GCC gebaut: 33/33 Tests der
  Anwendungsschicht und von MCP.

## Grenzen

- Kein SSE und keine Server-Benachrichtigungen; für Werkzeuge reicht die direkte Antwort.
- Sitzungen leben im Speicher; nach einem Neustart von Völund initialisiert der Client neu.
