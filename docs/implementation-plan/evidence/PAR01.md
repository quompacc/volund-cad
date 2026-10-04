# PAR01 – Dokumentparameter: Modell, Bindung, Persistenz und Neuberechnung

**Status:** umgesetzt am 04.10.2026, unter Windows geprüft. Die Abnahme folgt nach einem
grünen Linux-CI-Lauf. Konzept: [parameter-table.md](../../parameter-table.md).

## Umsetzung

- **Modell (`DocumentParameters.hpp`):**
  - `ParameterId` (stabil, unabhängig vom Namen) und `ParameterTable` mit Name, Einheit
    (`length` in mm, `angle` in Grad), Wert und Kommentar.
  - Namensregeln: Buchstabe am Anfang, danach Buchstaben, Ziffern oder `_`. Eindeutig
    ohne Rücksicht auf Groß-/Kleinschreibung. Einheiten und reservierte Wörter (`mm`,
    `deg`, `pi` …) sind ausgeschlossen.
- **Bindung:**
  - `SketchConstraint::parameter` bindet Länge, Abstand, Radius, Durchmesser,
    Punktabstände und Winkel; `ExtrusionParameters::distanceParameter` bindet die
    Extrusionsdistanz.
  - Der Parameterwert wird in `value`/`angleValue` gespiegelt, deshalb bleiben Solver und
    Features unverändert.
  - `validateParameterBindings` weist unbekannte Parameter, falsche Einheiten und
    Bindungen an Constraints ohne Wert ab. Die Prüfung läuft bei jedem Commit und beim
    Lesen.
- **Undo:**
  - Die Tabelle gehört zum Dokumentzustand (`DocumentSnapshot`, `ModelDocument::State`).
    Jede Änderung ist damit ein normaler Undo-Schritt.
  - Undo senkt die ID-Vergabe der Parameter nicht ab, wie bei Feature-IDs.
- **Neuberechnung:**
  - `CadSession::setParameterValue` öffnet eine Transaktion, spiegelt den Wert in alle
    Bindungen, löst betroffene Skizzen neu und rechnet abhängige Features neu.
  - Der FeatureGraph löst Basisebenen-Skizzen mit gebundenen Maßen bei der Auswertung neu.
    Ist eine Folge unlösbar, erscheint sie als `committedWithErrors` mit Fehlerstatus der
    Skizze; der neue Wert bleibt erhalten. Skizzen ohne Bindung verhalten sich wie bisher.
- **Session-Befehle:** `createParameter`, `renameParameter`, `setParameterComment`,
  `deleteParameter` (verweigert bei Verwendung, optional `unbindUses`),
  `bindSketchDimension`/`bindExtrusionDistance` (mit `nullopt` lösen; der aktuelle Wert
  bleibt fest), `parameterUseCount`.
- **Natives Schema 7:**
  - Die Tabelle liegt in `Main()` Slot 30, Bindungen als optionales Kind 9 des Constraints
    und Kind 3 der Extrusion.
  - Alles wird nur geschrieben, wenn vorhanden. Dokumente ohne Parameter behalten die
    ältere Kodierung, und Schema-6-Dateien öffnen ohne Parameter.
  - Schema ≤ 6 mit Tabelle oder Bindung wird abgewiesen.

## Prüfungen

| Test | Inhalt |
|---|---|
| `application.document_parameters` | Zwei Basisebenen-Skizzen (XY, XZ) mit Kreisradius und die Extrusion der ersten an A = 10 mm. A → 20 mm ändert Radien, Distanz und Volumen (1500π → 10000π mm³) in einem Undo-Schritt; Undo/Redo. Umbenennen erhält die Bindungen; doppelte und ungültige Namen werden abgewiesen. Ein Winkelparameter an Radius oder Distanz wird abgewiesen. Löschen eines verwendeten Parameters wird verweigert, die Tabelle bleibt gleich. Speichern/Öffnen ergibt dieselbe Tabelle, Bindungen und Volumina, und die Bindungen bleiben wirksam. Lösen behält den festen Wert. Ein Radius im Widerspruch zum festen Durchmesser ergibt `committedWithErrors` mit Fehlerstatus der Skizze; `reject` verweigert, Undo stellt her |
| `document.native_parameters` | Rundlauf mit und ohne Bindungen; Schema-6-Grenze; leere Tabelle wird nicht geschrieben; beschädigte Namen, Einheiten, IDs, Wasserzeichen und Anzahlen werden abgewiesen; gelöschter gebundener Parameter und falsche Einheit werden vor dem Schreiben abgewiesen |
| `document.native_metadata` | Schema 8 wird abgewiesen, Schema 7 ist aktuell |

- **Windows:** 112/112 Tests bestanden (Debug, MSVC, seriell).
- **Linux:** CI-Lauf ausstehend.

## Grenzen

- Keine Oberfläche (PAR02) und keine Ausdrücke (spätere Ausbaustufe).
- Ändert der Skizzeneditor ein gebundenes Maß direkt, weicht der Wert vom Parameter ab,
  bis PAR02 die Eingabe `=A` und das Lösen der Bindung im Maßfeld anbietet.
- Bei gestützten Skizzen ohne Support-Bindung (Altbestand) löst erst die Auswertung neu,
  die gespeicherte Geometrie folgt beim nächsten Bearbeiten.
