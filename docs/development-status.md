# Entwicklungsstand – 12. September 2026

## Verbindliches Produktziel

Völund wird als natives parametrisches CAD in C++20 mit Qt 6 und OpenCASCADE
Technology für FreeBSD mit Meridian entwickelt. Völund bleibt dabei eine
eigenständige Wayland-Anwendung. FreeCAD bleibt Referenz und Verhaltensorakel, ist aber
keine Runtime-Abhängigkeit. Python, WebView und eine browserbasierte CAD-Oberfläche
gehören nicht zum Produktpfad.

## Heute bestätigter Funktionsstand

- Der Qt-eigene OpenGL-Kontextpfad und das native Windows-Viewport-Backend bauen.
- Auswahl hebt die ganze Fläche sichtbar hervor.
- Mehrere Körper können getrennt ausgewählt und transformiert werden.
- Move und Rotate sind in einem Manipulator kombiniert; lineares und winkliges
  Raster sowie Live-Werte funktionieren.
- Box und Zylinder sind parametrische Features. Der Zylinder unterstützt Join, Cut,
  Intersect und Separate.
- Der Verbindungsmodus ist als globale, auswahlunabhängig bedienbare Toolbar sichtbar.
  Er ändert die aktuelle letzte Zylinderoperation und ist zugleich die Vorgabe für
  den nächsten Zylinder.
- `Separate` löst die räumliche Abhängigkeit: Ein getrennter Zylinder behält seine
  Weltposition, wenn sein früherer Eingabekörper verschoben wird. Bei `Join` bilden
  Würfel und Zylinder dagegen einen gemeinsamen Transformationskörper; die Auswahl
  jedes beitragenden Features aktiviert Move für dasselbe sichtbare Ergebnis.
- Das Dokument besitzt stabile Feature-IDs, Undo/Redo und exakte Modified-Erkennung.
- Neue, leere Dokumente sowie Open, Save und Save As im eigenen `.volund`-Format
  funktionieren einschließlich Speichern/Laden von Sketch und Extrusion.
- Auf planaren Flächen kann ein Rechteck gezeichnet und direkt per Push/Pull
  extrudiert werden. Wiederholte Extrusionen bleiben numerisch verbunden.
- Sketch und Extrusion sind intern getrennte parametrische Features. Die
  Extrusionsdistanz kann nachträglich editiert und neu berechnet werden.
- Sketches besitzen ein allgemeines parametrisches Datenmodell mit stabilen lokalen
  IDs für Linien, Rechtecke und Kreise. Mehrere Elemente und mehrere geschlossene
  Profile werden erkannt und gemeinsam extrudiert; offene Elemente bleiben erhalten.
- Wird ein Sketch im Modellbaum ausgewählt, stellt der Viewport alle enthaltenen
  Elemente als getrennte OCCT-Präsentationen dar. Linien, Rechtecke und Kreise sind
  über ihre stabilen Element-IDs einzeln pickbar; die Auswahl wird farblich und im
  Eigenschaftenbereich angezeigt.
- Der eigenständige Sketch-Editor startet auf einer ausgewählten planaren Fläche und
  erzeugt nacheinander Linien, Rechtecke und Kreise im selben temporären Sketch.
  Werkzeugwechsel erhalten bestätigte Elemente; lineares Snapping und Live-Preview
  gelten für alle drei Werkzeuge. Elemente sind bereits während des Editorlaufs über
  ihre stabilen IDs einzeln auswählbar.
- OK übernimmt den vollständigen Sketch als genau eine Dokumenttransaktion. Cancel
  und Escape hinterlassen weder Feature noch Undo-Eintrag. Rein offene
  Liniengeometrie ist ein gültiger speicherbarer Sketch; erst die Extrusion verlangt
  mindestens ein geschlossenes Profil.
- Ein ausgewählter Sketch kann über `Edit Sketch` erneut geöffnet werden. Vorhandene
  Elemente behalten ihre IDs, lassen sich auswählen, in sketchlokalem X/Y
  verschieben oder löschen; neue Elemente verwenden die fortgesetzte ID-Sequenz.
  OK recomputiert abhängige Profile und Extrusionen in einer Transaktion, Cancel
  verwirft sämtliche temporären Änderungen. Undo/Redo und Save/Load sind für diesen
  vollständigen Bearbeitungsablauf automatisiert geprüft.
- Im laufenden Bearbeitungsmodus kann ein ausgewähltes Element direkt im Viewport
  gezogen werden. Die Bewegung wird auf die Sketchebene projiziert, verwendet das
  lineare Raster und synchronisiert die X/Y-Livewerte. Der Drag verändert weiterhin
  nur den temporären Editorzustand; erst OK schreibt die Transaktion.
- Für ausgewählte Linien ist die exakte Länge im Tool-Panel editierbar. Horizontale
  und vertikale Richtungsconstraints richten die Linie längenerhaltend um ihren
  Startpunkt aus, bleiben beim Verschieben aktiv und werden mit Undo/Redo sowie
  Save/Load erhalten.
- Neu gezeichnete Rechtecke sind keine unteilbaren Makroelemente mehr. Der Editor
  erzeugt vier einzeln auswählbare Linien mit vier Horizontal-/Vertikal- und vier
  Koinzidenzconstraints. Alte gespeicherte Rechteckelemente bleiben kompatibel;
  ausgewählte Kreise bieten weiterhin ihren exakten Radius.
- Der Constraint-Kern ist FreeCAD PlaneGCS 1.1.1: als unveränderte, isolierte
  LGPL-Komponente eingebunden und über eine Völund-eigene C++-Grenze angesprochen.
  Längen, Radien sowie Horizontal/Vertikal besitzen stabile Constraint-IDs und
  werden im Sketch-Editor tatsächlich gelöst. Der Adapter unterstützt außerdem
  Koinzidenz, Parallelität und Rechtwinkligkeit, Freiheitsgrade und Konflikt-IDs.
  Endpunkte beziehungsweise Kreismittelpunkte werden per Klick gewählt, mit Strg
  mehrfach ausgewählt und können direkt gezogen werden. Das Tool-Panel aktiviert die
  drei Relationsbefehle kontextabhängig, zeigt Freiheitsgrade an und markiert
  widersprüchliche Geometrie rot; der letzte Constraint kann zur Wiederherstellung
  entfernt werden.
- Geschlossene Profile besitzen eine aus ihrer kleinsten stabilen Element-ID
  abgeleitete Profil-ID. Ein Klick auf eine Profilkante wählt die ganze Kontur;
  Strg-Klick erweitert oder reduziert die Auswahl. `Extrude Profiles` speichert
  genau diese Profil-IDs und extrudiert nur die ausgewählten Konturen.
- Constraints werden im Sketch-Viewport als farbige Textglyphen an ihrer Geometrie
  dargestellt. Richtungs- und Relationsconstraints erscheinen als kompakte Symbole;
  Längen und Radien zeigen ihren Wert in Millimetern. Ein Doppelklick auf eine
  Bemaßung öffnet die direkte Werteingabe und löst den Sketch sofort neu. Konflikte
  färben sowohl die betroffene Geometrie als auch ihre Constraint-Glyphen rot.
- Ein einzelner ausgewählter Linienendpunkt oder Kreismittelpunkt kann über zwei
  echte Koordinatenconstraints gesperrt und wieder entsperrt werden. Mit zwei
  Strg-ausgewählten Referenzen setzt `Point on object` den zuerst gewählten Punkt
  parametrisch auf die zweite Linie oder den zweiten Kreis; beide Varianten werden
  unmittelbar von PlaneGCS gelöst und als `[O]` im Viewport angezeigt.
- Neue Dateien verwenden `VOLUND 6`; bestehende `VOLUND 1`-Rechtecksketches,
  `VOLUND 2`-Linien, `VOLUND 3`-Richtungsconstraints und `VOLUND 4`-Extrusionen
  sowie `VOLUND 5`-Profilauswahlen werden beim Laden migriert. Alte Extrusionen
  behalten dabei ihre bisherige Semantik, alle geschlossenen Profile zu verwenden.
- Ein temporäres generisches Tool-Panel bietet Parameter, Live-Vorschau, OK und
  Cancel. Erst OK schreibt das Feature in das Dokument; Cancel oder Escape verwirft
  die Vorschau.

## Modul- und Bedienarchitektur

Die Oberfläche folgt dem bestätigten Fusion-Prinzip:

1. Arbeitsbereiche wie Solid, Surface, Mesh, Sheet Metal und Assembly sind Module.
2. Jedes Modul registriert Werkzeuggruppen, Schnellzugriffe und vollständige Menüs.
3. Häufige Werkzeuge erscheinen direkt; Spezialwerkzeuge bleiben im Gruppenmenü.
4. Ein gestartetes Werkzeug verwendet das generische temporäre Tool-Panel und die
   direkte Manipulation im Viewport.

Aktuell wird ausschließlich `Solid` als Produktmodul entwickelt. Andere
Arbeitsbereiche werden weder vorgetäuscht noch fest verdrahtet. Die Registry lehnt
doppelte Modul- und Command-IDs ab. Module werden zunächst statisch gelinkt; eine
dynamische Plug-in-ABI ist bewusst aufgeschoben.

## Entwicklungs- und Abnahmeverantwortung

Grundlegende CAD-Steuerung wird vom Entwicklungsagenten eigenständig als vollständiger
Bedienablauf umgesetzt und geprüft. Dazu gehören insbesondere Auswahl und Preselection,
Viewport-Navigation, Move/Rotate, Verbindungsmodi, Undo/Redo sowie Aktivierung und
Synchronisierung der Werkzeuge. Der Agent baut und testet beide Windows-Profile,
synchronisiert den Stand auf die FreeBSD-Testmaschine, führt dort die native
Wayland-Prüfung aus und startet den geprüften Build selbst.

Der Benutzer wird nicht als Ersatz für automatisierbare Funktions- oder
Regressionstests eingesetzt. Seine manuelle Abnahme konzentriert sich auf subjektive
Ergonomie, Bediengefühl und visuelle Wirkung. Nicht automatisierbare Hardware- oder
Compositorbeobachtungen werden ausdrücklich benannt, bevor eine solche Abnahme
angefordert wird.

## Technische Schutzmaßnahmen

- PlaneGCS ist als gepinnte Solver-Komponente hinzugekommen; Eigen und Boost.Graph
  sind ausschließlich seine Build-Abhängigkeiten. Herkunft, Lizenz und die
  Release-Pflichten sind in ADR 0005 und der Provenienzakte dokumentiert. Das
  schlanke FreeBSD-OCCT-Profil aktiviert FreeType für die nativen Constraint- und
  Bemaßungsglyphen, bleibt aber ohne Xlib, TBB, VTK und Qt 5.
- Beide Windows-Backends durchlaufen denselben Funktions- und Regressionsteststand.
- Zehn CTest-Tests prüfen zusätzlich den unveränderten PlaneGCS-Kern sowie den
  Völund-Adapter einschließlich Freiheitsgraden und stabiler Konfliktzuordnung.
- Regressionstests sichern sowohl die unabhängige Bewegung getrennter Körper als auch
  die gemeinsame Verschiebung eines Join-Ergebnisses einschließlich Undo.
- Die harte Grenze von 600 Zeilen je gepflegter Quelldatei bleibt aktiv.
- Der numerische Kontakt wiederholter Extrusionen wird mit einer ausschließlich nach
  innen liegenden OCCT-Überlappung von `0,0001 mm` stabilisiert; das Außenmaß bleibt
  unverändert.

## Aktueller Einstiegspunkt: FreeBSD/Meridian-Integration

Vor dem nächsten großen CAD-Slice wird der native Desktoppfad auf realer
FreeBSD-Hardware belegt:

1. Systembuild mit Clang, Qt 6 und einem schlanken OCCT-7.9.3-Profil;
2. nativer Wayland-/EGL/GLES-Kontext ohne `DISPLAY` und ohne stillen XCB-Fallback;
3. OCCT-Rendering, Picking, Resize, Skalierung und Fensterzustände;
4. Meridian-SSD, Theme-Umschaltung, Portal-FileChooser und XDG-App-Identität;
5. Zwischenprüfung auf einem ausgereiften FreeBSD-Wayland-Desktop und endgültige
   Abnahme unter Meridian;
6. Audit des FreeBSD-OCCT-Pakets auf Qt-5-/Xlib-Laufzeitkopplung.

Die Sway-Kontrollprüfung und Punkt 6 sind abgeschlossen: Der echte Wayland-Smoke-Test
bestand auf der Intel HD 620, und das projektlokale OCCT-Profil entfernt Xlib, TBB,
VTK und Qt 5 aus OCCT. Offen bleibt die Abnahme derselben Prüfkette unter Meridian.
Das FreeBSD-Qt6-Paket ist ein kombinierter Wayland/X11-Build und enthält deshalb trotz
erzwungener Wayland-QPA weiterhin ruhende ELF-Kanten zu `libX11` und `libGLX`.

Die ersten vollständigen Mehrfach-Element-Erzeugungs- und Bearbeitungs-Slices sind auf
beiden Windows-Backends und im nativen FreeBSD-Wayland-Controlpfad bestätigt. Sichtbare
Constraint-Glyphen, direkte Bemaßungsinteraktion, Punktsperren und Punkt-auf-Objekt
sind umgesetzt. Als nächste parametrische Ausbaustufe folgen Tangential-, Gleichheits-
und Symmetrierelationen.

Der direkte Push/Pull-Ablauf bleibt dabei als schneller Weg erhalten. Er ist eine
Bedienoberfläche über denselben parametrischen Sketch- und Extrusionsfeatures, kein
Ersatz für den vollständigen Sketch-Editor.
