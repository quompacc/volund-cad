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

- Keine neue Produktabhängigkeit wurde für diese Funktionen eingeführt.
- Beide Windows-Backends durchlaufen denselben Funktions- und Regressionsteststand.
- Sechs CTest-Tests prüfen Viewport-Start, Kernel, Dokumenthistorie,
  Anwendungs-Neuberechnung, Modulregistrierung und Quellgrößen.
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

Der vollständige parametrische Sketch-Modus ist der unmittelbar folgende CAD-Slice
und wird am nächsten Entwicklungstag fortgesetzt:

1. allgemeines Sketch-Datenmodell statt eines einzelnen Rechtecks;
2. eigenständige Linien, Rechtecke und Kreise;
3. Darstellung und Auswahl mehrerer Sketch-Elemente;
4. Erkennung geschlossener Profile;
5. Maße und geometrische Constraints;
6. freie Auswahl eines oder mehrerer Profile für nachfolgende Extrusionen.

Der direkte Push/Pull-Ablauf bleibt dabei als schneller Weg erhalten. Er ist eine
Bedienoberfläche über denselben parametrischen Sketch- und Extrusionsfeatures, kein
Ersatz für den vollständigen Sketch-Editor.
