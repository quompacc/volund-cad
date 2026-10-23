# K-GRENZE – OCCT-Algorithmen nur im Kernel

**Status:** Code und Prüfungen vom 22.10.2026; Abnahme nach grünem CI-Lauf. Das
Produktverhalten bleibt gleich.

## Auftrag und Entscheidung (22.10.2026)

- **Auftrag:** OCCT-Topologie- und Modellierungs-Header (BRep*, BRepBuilderAPI*,
  BRepAlgoAPI*, Geom*) und Fänge von `Standard_Failure` gibt es nur noch in `src/kernel`.
  - `src/viewport` bleibt für AIS/V3d ausgenommen.
  - Die Logik wandert in Kernel-Funktionen; Fehler meldet der Kernel als eigenen Typ.
  - Die Regel ist Pflichtprüfung der Schichtenprüfung (A04).
- **Umfang (Entscheidung des Nutzers):** Verboten sind die *Algorithmen*. `TopoDS_*`,
  `TopExp`, `TopTools` und `gp_*` dürfen als reine Datentypen durch alle Schichten reisen.
- **Befund vorab:** Die Bestandsaufnahme fand weit mehr als die drei genannten Stellen:
  - 23 Dateien in `src/features` (rund 4 500 Zeilen);
  - 3 in `src/application`, 2 in `src/desktop`;
  - die OCAF-Ablage in `src/document`.

## Änderung

### Neue Kernel-Schnittstellen (ohne OCCT-Algorithmen in ihren Köpfen)

- `KernelError.hpp`:
  - `KernelError` und die Operationen, neu `sketchGeometry`, `nativeDocument` und
    `evaluation`; der Kopf nennt keinen OCCT-Typ.
  - `translateOcctFailures` macht aus einem OCCT-Fehler `KernelError`. Speichermangel und
    OCCT-Programmierfehler reicht es wie bisher durch.
  - `translateEveryOcctFailure` übersetzt auch diese, für die Dokumentablage wie bisher.
  - `OcctCompatibility.hpp` behält nur `occ::handle` und `occtFailureDiagnostic` für Kernel
    und Viewport.
- `ShapeQueries.hpp`: Hüllquader und Mitte, Punkt-in-Fläche, Ebene und Außennormale einer
  ebenen Fläche, verschobene Kopie, Compound aus beliebig vielen Formen (auch leer).
- `EdgeQueries.hpp`:
  - Parameterbereich (Kurve und gespeichert), Punkt auf der Kante;
  - gleiche Grundkurve, Kante auf derselben Kurve mit anderem Bereich;
  - Linie oder Kreis als neutrale Beschreibung;
  - Anzeigefläche und Strecke der Bezugsgeometrie;
  - `MovedCopy`: verschobene Kopie samt Abbild jedes Teils.
- `PlanarCurve.hpp`:
  - 2D-Kurven der Skizze (Strecke, Kreis, Bogen, Ellipse, B-Spline) mit Parameterbereich,
    Wert, Ableitung, Toleranz und Bereichsabbildung;
  - Schnitt zweier Kurven;
  - Fläche aus einer geschlossenen Schleife in der Ebene (Regionsklassifikation).
- `SketchShapes.hpp`:
  - Raumkurven der Skizze mit den Parametern ihrer ebenen Kurve, Kanten und Ecken;
  - schrittweiser Draht (`WireBuilder`), geschlossener und verketteter Draht;
  - Flächen aus Draht, mit Löchern und mit einer Lochfläche (Ausrichtung geometrisch geprüft);
  - Außendraht, Gültigkeit, Flächeninhalt, „gleiche Kurve und gleicher Bereich“.

### Umgestellt

- **features:**
  - Skizzenauswertung, Schnitte, Regionen, Intervall- und Regionsflächen;
  - Extrusion, Fläche-Extrusion, Bohrung, Zylinder, Bezugsgeometrie, Spiegeln, Muster;
  - Kanten- und Konturherkunft, Skizzenstütze, Projektion, Referenzprüfung;
  - Feature-Graph.
- **application:** Bewegen und Gruppen-Bewegen (Hüllmitte), Bereichswahl (Punkt-in-Fläche).
- **desktop:** Griff der Verrundung (Compound); `guardCommand` fängt `KernelError`.
- **document:** Die OCAF-Ablage bleibt im Dokument, fängt aber nur noch `KernelError` über
  `translateEveryOcctFailure`.
- Die Meldungstexte bleiben gleich: Fehler der Auswertung tragen wie bisher den OCCT-Text.
- Die Kernel-Funktionen übernehmen die bisherigen OCCT-Aufrufe Schritt für Schritt
  (gleiche Konstruktoren, Parameter und Prüfungen).
  - Beim Punkt-in-Fläche-Test bleibt der Zustand „unbekannt“ erhalten, und mit ihm der alte
    Fehlerweg.

### Pflichtprüfung

`architecture.layer_includes` (A04, `tools/cmake/CheckLayerIncludes.cmake`) meldet jetzt
außerhalb von `kernel` und `viewport`:

- jedes Include eines OCCT-Algorithmus-Kopfs:
  `BRep*`, `Geom*`, `GC_*`, `GCE2d_*`, `GCPnts_*`, `gce_*`, `Standard_Failure`,
  `Standard_OutOfMemory`, `Standard_ProgramError`;
- jeden `catch` einer `Standard_*`-Ausnahme.

`architecture.layer_and_format_regression` belegt:

- Datentypen (`TopoDS`, `gp`) und PlaneGCS (`GCS.h`) außerhalb des Kernels sind erlaubt;
- `BRepBuilderAPI_MakeEdge` in features, `Geom2d_Circle` in application und ein
  `catch (const Standard_Failure&)` in desktop werden rot.

## Prüfungen

| Szenario | Nachweis |
|---|---|
| Regel über den ganzen Baum grün | `architecture.layer_includes` |
| Verstöße werden rot und benannt, Datentypen bleiben erlaubt | `architecture.layer_and_format_regression` |
| Verhalten unverändert: alle Nicht-Desktop- und Desktop-Tests auf Fedora, Nicht-Desktop-Tests auf Windows | volle Testläufe (siehe CI-Lauf) |

## Grenzen

- `include/voelund/kernel/OcctCompatibility.hpp` bindet `Standard_Failure.hxx` weiter ein. Er
  ist ein Kernel-Kopf, und außerhalb von Kernel und Viewport bindet ihn keine Datei mehr ein.
- Tests (`tests/`) dürfen OCCT weiter direkt nutzen, um Ergebnisse unabhängig zu prüfen.
