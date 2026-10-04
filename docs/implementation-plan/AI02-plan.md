# AI02 – Teilplan: gemeinsame zielgerichtete CAD-Aktionen

Stand 04.10.2026. Grundlage: [Kapitel 11](11-ki-fem-optimierung.md) (Aktionsvertrag AI01),
[ADR 0010](../adr/0010-native-actions-and-analysis-extension.md), K12 in
[02-zielvertraege.md](02-zielvertraege.md), Abnahme A-ACTION-01 in
[08-abnahme.md](08-abnahme.md). Nutzerentscheidung vom selben Tag: AI02 direkt nach R10b,
der Assistent AI03 direkt danach (Claude per API, Adapter austauschbar).

Ziel: GUI, späterer Assistent (AI03) und optionaler MCP-Zugang (AI04) verwenden **eine**
typisierte Aktionsschicht im Anwendungskern. Kein Netzwerk, kein Modell-SDK und keine
Skripte in AI02.

## Bestand (geprüft am 04.10.2026)

- **Befehle:**
  - `CommandRegistry` (`include/voelund/workspaces/CommandRegistry.hpp`) liefert
    `CommandDescriptor`. `src/desktop/CommandActions.cpp` erzeugt daraus je eine QAction
    mit `commandId`.
  - Werkzeugleiste, Kontextleiste und Befehlssuche lösen dieselben QActions aus.
  - Die Auslöser (`MainWindowCommands.cpp:26-101`) rufen `CadSession` direkt auf oder
    öffnen interaktive Controller (Skizze, Extrusion).
- **`CadSession`** (`include/voelund/application/CadSession.hpp`):
  - Mit expliziten Zielen: `editSketchParameters`, `createSketchFeature`,
    `createDatumSketchFeature`, `createSketchExtrusion`, `createSketchProfileExtrusion`,
    `editCylinderParameters`, alle PAR01-Methoden und die Reparaturen. Die Reparaturen
    prüfen bereits einen Stand, das einzige Vorbild für `expectedStamp`.
  - Nur über die Auswahl: `editSelectedBox*`, `editSelectedExtrusionParameters`,
    `editSelectedFeaturePlacement`. `createCylinderFeature()` nimmt implizit das letzte
    Feature.
  - Alle Mutationen laufen über `finishCandidate`
    (`CadSessionTransactions.cpp:28-73`): Kandidat auswerten, dann ein Commit mit genau
    einem Undo-Schritt. Eine Vorschau ohne Veröffentlichung gibt es nicht.
- **Stand:**
  - `DocumentStamp{generation, revision}` steht in `SceneStatus`.
  - `generation` steigt bei Neu, Öffnen und Duplizieren. `revision` steigt bei jedem
    Commit, auch bei Undo und Redo (`NativeModelState.cpp:45,66`), nimmt also nie ab.
- **`CommandResult`:** `committed`, `committedWithErrors` oder `rejected`. Gründe sind
  `noChange`, `invalidInput`, `evaluationFailed` und weitere. Es gibt keine eigenen
  Gründe für veralteten Stand, fremdes Dokument, unbekanntes Ziel oder falsche Einheit.
  Es fehlen der neue Stand und die Liste erzeugter oder geänderter IDs.
- **Skizzeneditor:**
  - `SketchEditorSession` ist ein lokaler Entwurf mit eigener Historie. Beim Übernehmen
    wird der Stand geprüft, danach folgt genau ein `CadSession`-Aufruf.
  - Ein Aufrufer ohne GUI kann heute schon so arbeiten: `SketchParameters` bauen →
    `createDatumSketchFeature` → RegionIds über `identifySketchRegions` →
    `createSketchProfileExtrusion`.
  - `bindSelectedSupport` erwartet eine rohe OCCT-Fläche und ist für KI und MCP
    ungeeignet.
- **Kontextabfragen:**
  - Vorhanden: `features()`, `modelOutline()`, `featureStates()`,
    `sketchParameters(id)`, `parameterTable()`, `inspectExtrusionSelection`,
    `inspectSketchReferences`.
  - Es fehlen: verfügbare Aktionen (Fähigkeiten), eine Beschreibung der Einheiten und die
    Regionen je Skizze als Session-API.

## Unterpakete

Reihenfolge verbindlich. Jedes Unterpaket hat einen eigenen Nachweis
(`evidence/AI02a.md` …), einen grünen CI-Lauf und einen lokalen Windows-Lauf.

### AI02a – Request, Result, Kontext und Standprüfung

- `application::ActionRequest`:
  - Felder: `requestId`, `DocumentId`, `DocumentStamp expected`, Aktion als `std::variant`
    typisierter Aktionen mit expliziten IDs.
  - Erste Aktionen: Parameter setzen, umbenennen, binden (PAR01), Skizze erzeugen oder
    ändern, Profil-Extrusion erzeugen, Extrusionsdistanz ändern.
- `ActionResult`:
  - Enthält das `CommandResult`, den neuen Stand, erzeugte und geänderte IDs.
  - Typisierte Gründe: `staleStamp`, `foreignDocument`, `unknownTarget`, `nonFinite`,
    `invalidUnit`, `duplicateRequest`, `requestConflict`.
- `ActionContext` (nur lesend): Gliederung, Featurezustände, Parametertabelle, Regionen
  je Skizze mit RegionId und Fläche, Einheiten (mm, rad), verfügbare Aktionen je Ziel.
- Prüfung vor jeder Transaktion in der AI01-Reihenfolge: Namespace, Stand, Existenz,
  Fähigkeit, Einheit, Endlichkeit, Referenzauflösung.
- **Test:** Jede Ablehnung lässt Stand, Undo und Dokument unverändert.

### AI02b – Mutationen mit expliziten Zielen

- Neue ID-Varianten für Box-, Extrusions- und Platzierungsänderungen sowie
  `createCylinderFeature(input)`. Die `Selected`-Methoden delegieren dorthin, wie es
  `editSelectedCylinderParameters` schon tut.
- Ein Dispatcher bildet die Aktionen auf diese Methoden ab.
- **Test:** A-ACTION-01, Teil „Auswahlwechsel“. Ein Request für Körper A ändert nur A,
  auch wenn die GUI inzwischen B gewählt hat.

### AI02c – Kandidat, Vorschau, atomarer Commit, Wiederholung

- `finishCandidate` wird geteilt: Auswerten als Vorschau ohne Veröffentlichung, dann
  Commit mit erneuter Standprüfung.
- Mehrteilige Aktion auf **einer** `DocumentTransaction` (Vorbild:
  `createSketchExtrusion`, `repairSketchSupports`), z. B. Skizze + Regionen + Extrusion.
  Die RegionIds werden aus den Kandidaten-Elementen berechnet.
- Tabelle `requestId → (Inhaltshash, Ergebnis)` je Generation, begrenzt. Dieselbe ID mit
  gleichem Inhalt liefert das alte Ergebnis ohne neue Mutation; mit anderem Inhalt wird
  sie abgewiesen.
- **Tests:**
  - Fehler in Schritt 2 hinterlässt nichts.
  - Genau ein Undo; Redo ergibt denselben Stand.
  - Wiederholung derselben requestId und geänderter Inhalt unter derselben ID.
  - Eine Nutzeränderung während der Vorschau lässt den alten Commit scheitern.

### AI02d – Anbindung der Shell

- Die Auslöser in `SolidWorkspaceModule`/`MainWindowCommands`,
  `SketchExtrusionToolController::accept` und `SketchEditorSession::tryAccept` bauen
  Requests aus Auswahl bzw. gespeichertem Stand und rufen den Dispatcher auf.
- Der Skizzenentwurf bleibt lokaler Werkzeugzustand.
- **Test:** GUI-Weg und direkter Aufruf liefern denselben Dokumentzustand und Status.
- Keine sichtbare UI-Änderung, daher keine Aufnahmen nötig.

### AI02e (optional) – Fähigkeiten vereinheitlichen

- `ShellState`-Regeln (z. B. `canAddCylinder`) delegieren an die Fähigkeitsprüfung der
  Aktionsschicht. Nur wenn AI02a–d hier Duplikate hinterlassen.

## Abnahme A-ACTION-01 → Unterpaket

| Punkt | Unterpaket |
|---|---|
| Auswahlwechsel ändert das Ziel nicht | AI02b |
| GUI und direkter Aufruf liefern gleiche Resultate | AI02d |
| Fremdes Dokument, alter Stand, fehlendes Ziel, NaN/Inf, falsche Einheit: keine Mutation, kein Undo | AI02a |
| Vorschau ändert das aktive Dokument nicht; Nutzeränderung macht den alten Commit ungültig | AI02c |
| Fehler in Schritt 2 hinterlässt nichts; genau ein Undo; Redo identisch | AI02c |
| requestId-Wiederholung ohne zweite Mutation; geänderter Inhalt abgewiesen | AI02c |
| Unabhängige fehlerfreie Körper bleiben zugänglich | AI02c (Bestand von `finishCandidate`) |

## Bewusst nicht in AI02

- Modelladapter, Netzwerk, API-Schlüssel und Chat-UI: Das ist AI03.
- MCP-Transport: Das ist AI04.
- Skizzen auf Körperflächen ohne GUI: Nötig ist ein Flächenbezug über `SubshapeRef` statt
  einer rohen OCCT-Fläche. Das wird ein eigenes Unterpaket, sobald AI03 es braucht.
- Neue Modellierungsfunktionen.

## Offene Punkte für den Start

- Geklärt in [AI02a](evidence/AI02a.md): `createDatumSketchFeature` löst keine
  Constraints, es übernimmt die Geometrie wie übergeben. Aufrufer ohne Editor liefern
  gelöste Geometrie.
- Für AI02d gibt es noch kein Testgerüst, das GUI-Weg und Direktaufruf vergleicht.
  Vorbild: `tests/desktop/V01FlowTests.cpp`.
- Lebensdauer der Wiederholungstabelle: je Generation, nicht über Speichern und Öffnen
  hinweg.
