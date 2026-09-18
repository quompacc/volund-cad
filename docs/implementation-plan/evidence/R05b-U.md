# R05b-U – Stabile Kantenreferenzen im Skizzeneditor

18.09.2026. Plattformübergreifend abgenommen.
R05b/R05 sind [insgesamt abgenommen](R05.md). Grundlage: [R05b-P](R05b-P.md).

## Produktverhalten

CadSession projiziert die eindeutig belegten Randkanten der ausdrücklich gebundenen
Stützfläche. Derselbe Resolver prüft Namespace, Herkunft, History, analytischen
Parameter und tatsächliche Flächenzugehörigkeit für Recompute und Editor.
Anzeige und Fangpunkte werden über den Desktop-Adapter an den Viewport übergeben.
Der Viewport erfindet für gebundene Skizzen keine Segmentnummern mehr.

Die Auswahl bietet Anfang, Ende und den analytischen Parameter 0,5 jeder benannten
Kante. Diese drei Fangpunkte sind unabhängig von der Anzeigedichte. Bei Kreisen ist
0,5 ein Punkt auf dem Kreis, kein interpolierter Sehnenmittelpunkt. Getrimmte
ursprüngliche Parameter werden nicht ersetzt; nicht belegte Kanten sind nicht
als stabile Referenz auswählbar. Freies Anklicken beliebiger Kurvenparameter ist
in diesem Bedienblock nicht enthalten. Der bestehende Legacy-Editor bleibt für
alte ungebundene Dokumente erhalten; Migration und Reparatur bleiben R06.

SketchEditorSession nimmt typisierte Referenzen an und erzeugt gemeinsam zwei
Koordinatenconstraints. Vor dem Solver werden ihre Koordinaten analytisch neu
aufgelöst. Fremde, fehlende oder veraltete Referenzen werden ohne Constraintmutation
abgewiesen. Im Controller wird die komplette Elementanlage zurückgenommen, wenn
ein gewünschtes Attachment fehlschlägt. Der Öffnungspfad unterstützt bestehende
Schema-3-Skizzen. Ein vorhandener Fehler bei der Weitergabe der Referenz des zweiten
Linienpunkts ist in Vorschau und Klick korrigiert.

Start/Öffnen des Editors liegt zur Einhaltung der 600-Zeilen-Grenze in einer eigenen
Datei. Kein neues Persistenzschema, keine zusätzliche Runtime, kein KI-/FEM-Code.

## Nachweise

- Vollständiger Windows-Build und **88/88 Tests bestanden**, 107,87 s.
  [JUnit](R05b-U-artifacts/windows-ctest.xml), [Log](R05b-U-artifacts/windows-ctest.log).
- Neuer Test `desktop.projected_edge_flow`: echte Viewport-Mausklicks für beide
  Linienenden, Desktop-Adapter und Controller, Ablehnung mit Entwurfs-Rollback,
  native Save/Open-Runde, Dokument-Undo/Redo, Recompute und Wiederöffnen,
  fehlende Herkunft, veralteter Entwurf sowie analytische Kreisnaht/Mitte.
- Anzeigedichten 4 und 64 liefern dieselben zwölf Fangreferenzen der Box-Stützfläche.
- 337 Quelldateien im Größencheck bestanden; Diffprüfung bestanden.
- [Windows-Aufnahme](R05b-U-artifacts/windows-ui/projected-edge-editor.png) betrachtet:
  geöffneter Editor, violette projizierte Randkanten, daran gebundener grüner
  Linienanfang und zwei verbleibende Freiheitsgrade sichtbar. Keine neue UI-Gestaltung.
- 358 Quell-/Fixture-Hashes; 19 Dateien im Delta gegenüber R05b-P.
  [Transferreview](R05b-U-artifacts/transfer-review.md).

FreeBSD/OCCT 7.9.3: Build und **85/85 Tests bestanden**, 31,21 s.
[JUnit](R05b-U-artifacts/r05bu-ctest.xml), [Log](R05b-U-artifacts/r05bu-ctest.log).
Alle **358 Quell-/Fixture-Hashes** stimmen zwischen Manifest, Windows und FreeBSD
überein: [Vergleich](R05b-U-artifacts/final-source-verification.txt).
Die [Wayland-Aufnahme](R05b-U-artifacts/r05bu-ui/projected-edge-editor.png) wurde
selbst betrachtet: violette Randkanten, grüner gebundener Linienanfang, geöffneter
Editor und zwei verbleibende Freiheitsgrade sichtbar. Plattformtypische Unterschiede
bei Hintergrund, Schrift und Auswahlmarkierungen ändern den geprüften Bedienweg nicht.

Nach anfänglicher automatischer Ablehnung hat der Nutzer den konkret beschriebenen R05b-U-Transfer mit „weiter“ freigegeben. 19 Dateien und Prüfdaten wurden übertragen. Build und Prüfskript sind erfolgreich beendet; Logs, Hashes und UI-Aufnahme wurden zurückgeholt. Kein Commit/Push.
