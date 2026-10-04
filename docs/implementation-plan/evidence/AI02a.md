# AI02a – Request, Result, Kontext und Standprüfung

**Status:** abgenommen am 04.10.2026 nach gr�nem CI-Lauf 375. Teilplan: [AI02-plan.md](../AI02-plan.md).

## Umsetzung

- **Aktionen (`include/voelund/application/Actions.hpp`):**
  - `ActionRequest` mit `requestId`, `DocumentId`, erwartetem `DocumentStamp` und einer
    Aktion als `std::variant` mit expliziten Ziel-IDs.
  - Erste Aktionen: Parameter anlegen, Wert setzen, umbenennen; Skizzenmaß und
    Extrusionsdistanz binden oder lösen; Basisebenen-Skizze anlegen; Skizze ändern;
    Profil-Extrusion über RegionIds; Extrusionsdistanz ändern.
  - Zahlenwerte tragen ihre Einheit (`Quantity`): Längen in mm, Skizzenwinkel in rad,
    Winkelparameter in Grad wie in der Parametertabelle. Es wird nichts umgerechnet.
- **`executeAction(CadSession&, const ActionRequest&)`:**
  - Prüfung vor jeder Transaktion in der AI01-Reihenfolge: Namespace, Stand, Existenz,
    Fähigkeit, Einheit, Endlichkeit, Referenzauflösung. Bei mehreren Befunden gewinnt die
    erste Kategorie.
  - Angenommene Requests laufen als genau ein bestehender `CadSession`-Befehl. Dessen
    `CommandResult` bleibt unverändert im `ActionResult`; lehnt der Befehl ab
    (z. B. doppelter Name, keine Änderung), ist `rejection == none`.
  - `ActionResult` enthält den neuen Stand sowie erzeugte und geänderte Feature- und
    Parameter-IDs. Diese stammen aus dem Vergleich der Snapshots vor und nach dem Befehl,
    erfassen also auch gespiegelte Bindungen.
  - Die Gründe `duplicateRequest` und `requestConflict` sind angelegt und kommen mit AI02c.
- **Fähigkeiten:** eine gemeinsame Regel (`detail::availableActions`) für Kontext und
  Prüfung. Eine an einen Parameter gebundene Extrusionsdistanz bietet kein
  `setExtrusionDistance` an; sie folgt dem Parameter.
- **`describeActionContext(const CadSession&)`:** Dokument-ID, Stand, Einheiten,
  Features, Gliederung, Featurezustände, Parametertabelle, Regionen je Skizze (RegionId,
  Fläche in mm², Mehrdeutigkeit) und verfügbare Aktionen je Ziel.
- **`CadSession::editExtrusionParameters(FeatureId, …)`** und `extrusionParameters(id)`:
  explizites Ziel; `editSelectedExtrusionParameters` delegiert dorthin.
- **Offener Startpunkt geklärt:** `createDatumSketchFeature` löst keine Constraints, es
  übernimmt die Geometrie wie übergeben. Ein Aufrufer ohne Editor liefert gelöste
  Geometrie; das gilt auch für `editSketch`.

## Prüfungen

| Test | Inhalt |
|---|---|
| `application.action_requests` | Aufbau nur über Requests: Parameter A (mm) und W (Grad), Kreisskizze, Profil-Extrusion über die RegionId aus dem Kontext, Distanz 15, Bindung an A, A → 20; Volumen und gemeldete IDs je Schritt. Kontext: Stand, Einheiten, Region mit Fläche 100π, angebotene Extrusion. Ablehnungen: fremdes Dokument, alter Stand (Revision, Generation), unbekanntes Feature, unbekannter Parameter, unbekanntes Skizzenmaß, falsche Zielart, gebundene Distanz, falsche Einheiten (auch bei Bindungen), NaN/±Inf, unauflösbare Region, sowie die Reihenfolge bei mehreren Befunden. Nach jeder Ablehnung sind Stand, Dokument-Snapshot, Undo und Redo gleich; das nächste Undo nimmt weiterhin A → 20 zurück. Ein vor Redo vorbereiteter Request ist danach veraltet |

- **Windows:** 110/110 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf 375 auf `ac70a94`, gr�n. Lauf 374 auf `73e04b0` war rot: GCC meldete die
  unvollst�ndige Aggregat-Initialisierung von `ActionResult`
  (`-Werror=missing-field-initializers`), MSVC nicht. `ac70a94` initialisiert alle Felder.

## Grenzen

- Ziele mit Box, Zylinder und Platzierung sowie der Dispatcher für die Selected-Methoden
  folgen in AI02b; Vorschau, mehrteilige Aktionen und Wiederholungsschutz in AI02c.
- Endlichkeit wird für die Zahlenwerte der Aktionen geprüft. Skizzengeometrie prüft
  weiterhin die Dokumentvalidierung; sie lehnt als `invalidInput` ab, ebenfalls ohne
  Mutation.
- Die Shell ruft noch nicht über die Aktionsschicht auf (AI02d).
