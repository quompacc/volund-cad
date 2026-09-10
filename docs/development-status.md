# Entwicklungsstand – 10. September 2026

## Verbindliches Produktziel

Völund wird als natives parametrisches CAD in C++20 mit Qt 6 und OpenCASCADE
Technology entwickelt. FreeCAD bleibt Referenz und Verhaltensorakel, ist aber keine
Runtime-Abhängigkeit. Python, WebView und eine browserbasierte CAD-Oberfläche gehören
nicht zum Produktpfad.

## Heute bestätigter Funktionsstand

- Der Qt-eigene OpenGL-Kontextpfad und das native Windows-Viewport-Backend bauen.
- Auswahl hebt die ganze Fläche sichtbar hervor.
- Mehrere Körper können getrennt ausgewählt und transformiert werden.
- Move und Rotate sind in einem Manipulator kombiniert; lineares und winkliges
  Raster sowie Live-Werte funktionieren.
- Box und Zylinder sind parametrische Features. Der Zylinder unterstützt Join, Cut,
  Intersect und Separate.
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

## Technische Schutzmaßnahmen

- Keine neue Produktabhängigkeit wurde für diese Funktionen eingeführt.
- Beide Windows-Backends durchlaufen denselben Funktions- und Regressionsteststand.
- Sechs CTest-Tests prüfen Viewport-Start, Kernel, Dokumenthistorie,
  Anwendungs-Neuberechnung, Modulregistrierung und Quellgrößen.
- Die harte Grenze von 600 Zeilen je gepflegter Quelldatei bleibt aktiv.
- Der numerische Kontakt wiederholter Extrusionen wird mit einer ausschließlich nach
  innen liegenden OCCT-Überlappung von `0,0001 mm` stabilisiert; das Außenmaß bleibt
  unverändert.

## Nächster Einstiegspunkt

Als nächstes folgt der vollständige parametrische Sketch-Modus auf Basis der bereits
vorhandenen Modul-, Command- und Tool-Panel-Infrastruktur:

1. allgemeines Sketch-Datenmodell statt eines einzelnen Rechtecks;
2. eigenständige Linien, Rechtecke und Kreise;
3. Darstellung und Auswahl mehrerer Sketch-Elemente;
4. Erkennung geschlossener Profile;
5. Maße und geometrische Constraints;
6. freie Auswahl eines oder mehrerer Profile für nachfolgende Extrusionen.

Der direkte Push/Pull-Ablauf bleibt als schneller Weg erhalten. Er ist eine
Bedienoberfläche über denselben parametrischen Sketch- und Extrusionsfeatures, kein
Ersatz für den vollständigen Sketch-Editor.
