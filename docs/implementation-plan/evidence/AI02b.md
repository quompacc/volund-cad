# AI02b – Mutationen mit expliziten Zielen

**Status:** abgenommen am 04.10.2026 nach grünem CI-Lauf 378. Teilplan: [AI02-plan.md](../AI02-plan.md).

## Umsetzung

- **`CadSession` mit expliziten Zielen:**
  - `createBoxFeature(BoxDimensions)`, `createCylinderFeature(input, mode)`,
    `editBoxDimensions(id, …)`, `editBoxParameters(id, …)`, `editFeaturePlacement(id, …)`
    (neu in `CadSessionTargetedEdits.cpp`), dazu `canAddCylinder(input)` und
    `placementFeature(id)`.
  - Die bisherigen Varianten delegieren: `createBoxFeature()` mit 20 × 20 × 40,
    `createCylinderFeature(mode)` auf das letzte Feature, die `Selected`-Methoden auf die
    gewählte ID. Ihr Verhalten bleibt gleich.
  - Die gezielten Änderungen lassen die Auswahl unverändert; neue Features werden wie
    bisher gewählt.
- **Aktionen:** `CreateBox`, `SetBoxDimensions`, `CreateCylinder` (Eingangsfeature und
  Verknüpfungsart), `SetCylinderSize` sowie `SetPlacement` mit Verschiebung in mm und
  Drehung in rad. `SetPlacement` bewegt das Feature, das das Ergebnis platziert: Bei
  einem verbundenen Zylinder ist das sein Eingangskörper.
- **Fähigkeiten:** `availableActions` kennt jetzt auch den Laufzeitzustand. Ein Zylinder
  braucht ein aktuelles Ergebnis des Eingangsfeatures, eine Platzierung einen
  Platzierungseigner. Der Kontext bietet nur an, was die Prüfung annimmt.
- **Dispatcher:** `executeAction` bildet alle Aktionen auf genau einen dieser Befehle ab.

## Prüfungen

| Test | Inhalt |
|---|---|
| `application.action_targets` | A-ACTION-01 „Auswahlwechsel“: Zwei Boxen A und B, B gewählt und zuletzt angelegt. Maße und Platzierung von A ändern nur A; ein Zylinder mit Eingang A hängt an A, nicht an B. Größe des Zylinders ändern, Platzierung über den Zylinder bewegt A. Jedes Ergebnis meldet genau das geänderte Feature, B bleibt gewählt. Ablehnungen: Boxmaße am Zylinder, Zylindergröße an der Box, unbekannter Eingang, Drehung in Grad, NaN; Dokument und Stand bleiben gleich, B bleibt gewählt |
| bestehende Tests | `creation_results`, `command_transactions`, `recompute_history` und die Desktop-Tests nutzen weiter die delegierenden Varianten |

- **Windows:** 111/111 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 378 auf `8d98d3f`, grün. Vorab auf Fedora mit GCC gebaut: 30/30 Tests der
  Anwendungsschicht.

## Grenzen

- Die Shell ruft weiterhin die `Selected`-Methoden direkt auf; die Umstellung auf Requests
  folgt in AI02d.
- Box- und Zylinderparameter werden über einzelne Maße geändert; Ergebnisart und
  Platzierung über eigene Aktionen. Ein Wechsel der Zylinder-Verknüpfungsart als Aktion
  fehlt noch.
