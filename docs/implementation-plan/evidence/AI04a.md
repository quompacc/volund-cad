# AI04a – JSON-Codec und Werkzeugkatalog

**Status:** umgesetzt, CI-Lauf ausstehend. Teilplan: [AI04-plan.md](../AI04-plan.md).

## Umsetzung

- **Modul `src/mcp`** (`Voelund::Mcp`, nur Qt Core und die Anwendungsschicht). Noch ohne
  Netz.
- **Werkzeugkatalog Version 1** (`toolCatalog`): `get_context` und 15 Aktionswerkzeuge
  (Parameter anlegen, setzen, umbenennen, binden; Skizze anlegen, ändern; Profil-Extrusion;
  Skizze + Extrusion; Distanz; Box; Boxmaße; Zylinder; Zylindergröße; Platzierung). Jedes
  hat Beschreibung und `inputSchema`; die Beschreibungen erklären Stand, Einheiten,
  Wiederholung mit derselben `requestId` und `preview`.
- **Dekodieren** (`decodeActionCall`):
  - Gemeinsame Felder `requestId`, `documentId`, `stamp`, optional `preview`.
  - Zahlen der Aktionen als `{value, unit}` mit `mm`, `deg` oder `rad`. Ob die Einheit
    passt, entscheidet wie beim Direktaufruf die Aktionsschicht (`invalidUnit`).
  - Skizzen auf Basisebenen mit Linie, Rechteck, Kreis und Bogen (Koordinaten in mm,
    Winkel in rad) und einer Teilmenge der Bedingungen: Maße sowie horizontal, vertikal,
    deckungsgleich, parallel, senkrecht, gleich, konzentrisch.
  - Ein an einen Parameter gebundenes Maß übernimmt dessen Wert; ein abweichender Wert
    wird abgewiesen.
  - Skizzen mit Bedingungen werden vor dem Anlegen gelöst. Der Client darf ungefähre
    Koordinaten liefern; eine unlösbare Skizze wird mit den widersprüchlichen Bedingungen
    abgewiesen.
  - `edit_sketch` behält Ebene, Basisebene und Stützbindung der Skizze.
  - Jeder Formfehler nennt das Feld (`sketch.elements[0].type`) und ändert nichts.
- **Kodieren:** Ergebnis mit Status, Ablehnungsgrund, Diagnose, Stand, erzeugten und
  geänderten IDs, `repeated`, `preview` und Auswertungsfehlern. Der Kontext enthält
  Features mit ihren Parametern (Skizzen in der Form, die die Werkzeuge annehmen,
  Regionen mit ID und Fläche), Körper, Parameter und die Werkzeuge je Ziel.
- **`ToolHost`:** beantwortet `tools/call` als MCP-`CallToolResult` (strukturierter
  Inhalt plus derselbe JSON-Text). Mutationen laufen über einen `ActionDispatcher`;
  `changed` meldet jede veröffentlichte Änderung, nicht Vorschau oder Wiederholung.
- **Verhaltensänderung aus AI02c:** `CreateSketchExtrusion` ohne Regionen füllt
  verschachtelte Konturen jetzt abwechselnd wie in einer Zeichnung: außen gefüllt, eine
  Kontur darin ergibt ein Loch, eine Insel darin ist wieder gefüllt. Vorher wurden alle
  eindeutigen Regionen extrudiert, die Bohrung einer Platte wäre also zugeschüttet worden.

## Prüfungen

| Test | Inhalt |
|---|---|
| `mcp.tools` | Katalog: 16 Werkzeuge mit eindeutigen Namen und Schema. Nur über Werkzeugaufrufe: Parameter A = 8 mm, Platte 80 × 40 mit Bohrung, deren Radius an A gebunden ist (ungefähr 9 geliefert, auf 8 gelöst), Dicke 10; Volumen mit Loch; A → 10 ändert die Bohrung. Kontext mit Skizze, Regionen und Ebene. Wiederholung ohne zweite Änderung, Vorschau ohne Änderung. Ablehnungen wie beim Direktaufruf (falsche Einheit, unbekanntes Ziel, alter Stand) und Formfehler (fehlende `requestId`, falscher Typ, kaputte Region-ID, unbekannter Elementtyp, unlösbare Skizze, Ebene bei `edit_sketch`) mit Feldangabe; unbekanntes Werkzeug. Stand, Dokument und Änderungszähler bleiben jeweils gleich |
| `application.action_candidates` | Skizze + Extrusion mit einem Kreis wie bisher |

- **Windows:** 114/114 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf ausstehend. Vorab auf Fedora mit GCC gebaut: 32/32 Tests der
  Anwendungsschicht und `mcp.tools`.

## Grenzen

- Kein Transport; MCP-Protokoll und HTTP folgen in AI04b.
- Ellipsen und Bedingungen mit Zusatzwerten (z. B. symmetrisch, tangential) werden
  ausgegeben, aber noch nicht angenommen.
- JSON kennt kein NaN; `nonFinite` ist über MCP nicht erreichbar.
