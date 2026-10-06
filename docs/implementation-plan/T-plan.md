# Block A – Werkzeuglücken aus AI03c (T01–T03)

Stand 06.10.2026. Anlass: In der Abnahme A-AI-01 ([AI03c](evidence/AI03c.md)) konnte Claude
eine Rippe nicht als Teil der Platte bauen und hat dazu vier Lücken benannt.
Ziel: Diese Fälle gehen in der Oberfläche **und** über MCP (Aktionsschicht `Actions.hpp` →
`ToolCatalog.cpp`).

## Nutzerentscheidung vom 06.10.2026

- **Reihenfolge: Fläche zuerst.**
  - Eine Extrusion aus einer Skizze auf einer Körperfläche wird schon heute mit diesem
    Körper vereinigt. Nur Skizzen auf Basisebenen eröffnen einen neuen Körper.
  - Über MCP fehlte allein der Weg zur Fläche. T01 löst damit den Rippenfall ohne
    Schemaänderung und ohne neue sichtbare Oberfläche.
- **Verschieben von Skizzen- und Extrusionskörpern entfällt hier.** Es bleibt in M11
  (Platzierung aller Körper). Mit Fläche und Versatz lässt sich die Rippe direkt richtig
  platzieren.

## Pakete

| Paket | Inhalt | Gate |
|---|---|---|
| T01 | Skizze auf einer Körperfläche über MCP | G5 (wie AI04) |
| T02 | Ergebnis der Extrusion: neuer Körper / vereinigen / abziehen | G3, vorgezogener Teil von M02/M03 |
| T03 | Startversatz und Extrusion bis Fläche | G3, vorgezogener Teil von M02 |

Danach wird die Abnahme mit echtem Client (`voelund_assistant_panel_acceptance`) erneut
ausgeführt.

### T01 – Skizze auf einer Körperfläche über MCP

- `get_context` nennt je Körper:
  - `supportId`: das jüngste Ergebnis des Körpers;
  - `faces`: die ebenen Flächen, die eine Skizze benennen kann.
- Jede Fläche hat eine semantische Referenz nach
  [subshape-reference-contract.md](../subshape-reference-contract.md): Feature plus Rolle
  (`+z` eines Quaders, `end` eines Zylinders) oder Randelemente der Deckfläche einer
  Extrusion.
  - Kein Formindex verlässt Völund.
  - Dazu kommen das Skizzen-Koordinatensystem der Fläche (`origin`, `normal`,
    `xDirection`, `yDirection`) und ihre Ausdehnung darin (`min`, `max`).
  - Das System ist dasselbe, das ein Klick auf die Fläche in der Oberfläche ergibt.
- Neues Werkzeug `create_sketch_on_face` (`supportId`, `face`, `sketch` ohne Ebene).
  - Es nutzt die vorhandene Aktion `CreateSketchAction` mit Flächenbindung.
  - Eine Extrusion dieser Skizze vereinigt sich mit dem Körper.
- Skizzen auf Flächen nennen im Kontext ihre Fläche und ihr aktuelles Koordinatensystem.
- Fehlerfälle:
  - Eine unbekannte Fläche ist ein Argumentfehler (`face`).
  - Ein älteres Ergebnis eines Körpers ist ebenfalls ein Argumentfehler (`supportId`),
    weil eine Extrusion darauf den Körper nicht erreicht.
  - Ein unbekanntes Ziel wird abgewiesen (`unknownTarget`).
- **Test:** `mcp.tools`
  - Platte mit Bohrung, Rippe auf der Deckfläche: ein Körper, das Volumen stimmt.
  - Eine dickere Platte nimmt die Rippe mit.
  - Ein Quader bietet sechs Seiten an.
  - Fehlerfälle ohne Änderung.
- **Abnahme:** Rippenschritt des Abnahmehelfers verlangt einen Körper und +60 × 4 × 8 mm³.
- **Grenzen:**
  - Eine extrudierte Platte bietet nur ihre Deckfläche an. Boden- und Mantelflächen haben
    noch keine stabile Herkunft (ExtrusionSideKey im Vertrag, nicht umgesetzt).
  - Zylindermäntel sind nicht eben.

### T02 – Ergebnis der Extrusion

- Das Zielbild ist freigegeben: [ui-concept.md](../ui-concept.md), Ablauf Schritt 5
  („Ergebnis: neuer Körper, vereinigen, abziehen“).
- Schema: Operation und Zielkörper an `ExtrusionParameters`. Dazu kommen eine neue
  Schemaversion, eine Migration (alte Dateien behalten ihr heutiges Verhalten) und Tests
  für das Lesen alter Dateien.
- Panel (Nutzerentscheidung 06.10.2026, „Richtung + Ergebnis“):
  - Felder Distanz, Richtung (entlang / entgegen der Flächennormalen), Ergebnis (Neuer
    Körper / Vereinigen / Abziehen).
  - Vorbelegung: Skizze auf einer Fläche → Vereinigen, auf einer Basisebene → Neuer
    Körper.
  - Die Wahl von Abziehen stellt die Richtung sichtbar auf „entgegen“ (in das Material);
    das bleibt änderbar.
  - Ein Feld Zielkörper erscheint nur bei mehreren Körpern und einer Skizze auf einer
    Basisebene.
  - Aufnahmen werden dem Nutzer vorgelegt.
- MCP: `operation` an den Extrusionswerkzeugen.
- Bei getrennten Körpern gilt der Vertrag aus M03: ein getrenntes Vereinigen wird
  abgewiesen, keine stille Umdeutung.

### T03 – Startversatz und Extrusion bis Fläche

Nutzerentscheidung vom 06.10.2026: „Versatz + Bis Fläche“; die T02-Aufnahmen sind
freigegeben.

- Feld „Versatz“ unter Distanz: Längenmaß, Vorgabe 0 mm, auch negativ, an Parameter
  bindbar. Die Extrusion beginnt um den Versatz entlang ihrer Richtung verschoben.
- Endbedingung „Bis Fläche“: Eine ebene Fläche eines Körpers wird im Viewport bzw. über
  ihre Referenz aus `get_context` gewählt (wie T01). Die Extrusion endet auf ihr und folgt
  ihr bei Änderungen.
- Schemaänderung wie T02; Panel und MCP; Aufnahmen werden vorgelegt.
