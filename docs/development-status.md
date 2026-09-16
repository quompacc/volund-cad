# Entwicklungsstand – 12. September 2026

## Aktueller Stand – 16. September 2026

[Fortsetzungsnotiz](session-checkpoint-2026-09-16.md) und
[D10-Abschlussprüfung](implementation-plan/evidence/D10-review.md).
D10b2b hat die interne Radiantmigration abgeschlossen. D10c/D10d implementieren
Größen-/Toleranzrollen und exakten Boolean-Anschluss. Die aktuellen Suiten
bestehen nach [D11n](implementation-plan/evidence/D11n.md) mit **66/66 Windows** und **62/62 FreeBSD/Sway**.

**D10 ist auf Nutzerauftrag abgeschlossen; D11 ist begonnen.** Der in der Abschlussprüfung reproduzierte
ID-Überlauf ist durch Vorabprüfung des vollständigen ID-Bedarfs behoben und mit
Grenztests abgesichert. Die beschriebenen A-NUM-01-Fälle bestehen. D11a ergänzt
getrennte DocumentId-/BodyId-Typen und eine deterministische Legacy-Körperzuordnung.
D11b verhindert die Wiederverwendung veröffentlichter Feature-IDs nach Undo und
erhält den korrekten Inhalts-/Dirty-Vergleich. D11c trennt die Körper-ID in der
produktiven Auswertung vom letzten Shape-Erzeuger. Ihre bestehende Herkunfts-
zuordnung bleibt über Moduswechsel, Save-As und Open erhalten. Dauerhafte
Dokumentidentität, Duplikatsemantik und nativer Identitäts-/Schemaübergang bleiben offen.
D11d ergänzt die verlustfreie Dokument-ID-Feldkodierung und den isolierten
FreeBSD-OCCT-Build mit OCAF. Native Proben bestehen auf beiden Plattformen mit
je **8/8**, einschließlich bidirektionaler Speicherzyklen und Laufzeitprüfung.
D11e ergänzt Betriebssystem-Zufallsvergabe und einen unveränderlichen identifizierten
Snapshot: Revision und Wertkopie behalten die ID, explizites Duplizieren erzeugt
einen neuen Dokumentnamensraum. Diese Store-Schnittstelle ist geprüft; ihre
produktive New-/Open-/Save-As-/Duplikat-Anbindung bleibt offen.
D11f ergänzt den privaten nativen Dokumentkopf mit strikter Kennungs-/Schema-
und Identitätsprüfung. Native Speicherzyklen in beide Plattformrichtungen
bestehen. Feature-/Parameter-Schema und produktiver Dispatch fehlen weiterhin.
D11g bildet reine Box-Snapshots mit Parametern und getrennten Körper-/Feature-
Identitäten nativ ab. Beide Austauschrichtungen und die erneute BRep-Auswertung
sind geprüft. D11h erweitert dies um Zylinder aller vier Modi, Körpergruppen,
native Eingabereferenzen und eine unabhängige Feature-Reihenfolge. D11i ergänzt
Skizzenebenen, Elemente, IDs und Stützreferenzen. D11j ergänzt alle 26 Constraintarten
mit nativen Elementreferenzen, Maß-/Winkel- und Zusatzwerten sowie Solver-Parität.
D11k ergänzt Extrusionsdistanz, native Skizzeneingabe und geordnete Profilauswahl.
Alle vier aktuellen Featurearten sind im privaten Adapter abgebildet; die
Produktintegration bleibt offen.
D11k wurde mit `225e7d8` abgeschlossen. D11l ergänzt den gemeinsamen Schema-/
Identitätsnachweis auf beiden Plattformen. Nächster Block ist der produktive
Dokumentidentitäts-Lebenszyklus an der CadSession-/Snapshot-Grenze.
D11 bleibt insgesamt offen.
Der geprüfte D10-/D11e-Quellstand samt Belegen ist auf Nutzerauftrag in
`6737465afe6af30758147471035a5582e466342f` auf `main` committed. Ausgangspunkt
der Prüfläufe war `e8b65b8`; ältere Uncommittet-Vermerke in den Evidenzen
beschreiben diesen historischen Prüfzeitpunkt. Keine Terra-Übergabe, kein Push.
[Changelog](../CHANGELOG.md) und [Versionsregeln](versioning.md) sind angelegt;
die Basisversion bleibt `0.1.0`, ein Release-Tag ist noch nicht gesetzt.
FreeBSD/Sway zuerst, Meridian bleibt L07. A02/A04 und die noch nicht umgesetzten
Referenz-/DAG-/Persistenzpakete bleiben offen.

Die folgenden Funktionsangaben stammen aus dem ursprünglichen Übergabestand vom
12. September und sind zusammen mit den aktuellen Paket-Evidenzen zu lesen.

D11l: gemeinsame Feld-/Identitätsprüfung und gemischter Schematest vollständig
mit **65/65 Windows**, **61/61 FreeBSD/Sway** und beidseitigem Dateiaustausch geprüft.
[D11l-Prüfung und offene Integrationsgrenzen](implementation-plan/evidence/D11l.md).

D11m ergänzt den Dokumentidentitäts-Lebenszyklus in CadSession einschließlich
New, Legacy-Open, Save-As und explizitem Anwendungsduplikat. **66/66 Windows**,
**62/62 FreeBSD/Sway** bestanden; **294 Quellhashes identisch**.
Dokument-IDs werden weiterhin nicht in VOLUND 16 gespeichert; als nächster Block
folgt die D11-Abnahmematrix. D11m-Implementierung: `9931c10`; Desktop-Anbindung siehe D11n.
[D11m-Details](implementation-plan/evidence/D11m.md).

D11n bindet **File → Duplicate** an den bestehenden Desktop-Dokumentablauf an:
Original speichern, ohne Speichern duplizieren oder abbrechen; Fehlerdialoge für
New und Duplicate. **66/66 Windows**, **62/62 FreeBSD/Sway** bestanden; 294 Quellhashes identisch. [D11n-Belege](implementation-plan/evidence/D11n.md).

## Verbindliches Produktziel

Nutzerpräzisierung vom 16.09.2026: Mehrere unabhängige Körper direkt in einem
Dokument konstruieren, ohne eine Komponente oder Baugruppe vorbereiten zu müssen.
Der erste und weitere Körper verwenden denselben „Neuer Körper“-Workflow und sind
im Körperbereich einzeln bearbeitbar. Vertrag und geplante Abnahme A-BODY-03 sind
festgehalten; die bisherigen Mehrkörper-/Codec-Tests belegen noch nicht diesen
vollständigen UI-Ablauf. Umsetzung über S03, M01–M04 und R01 bleibt offen.

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
  IDs für Linien, Rechtecke, Kreise, Kreisbögen und Ellipsen. Mehrere Elemente und mehrere geschlossene
  Profile werden erkannt und gemeinsam extrudiert; offene Elemente bleiben erhalten.
- Wird ein Sketch im Modellbaum ausgewählt, stellt der Viewport alle enthaltenen
  Elemente als getrennte OCCT-Präsentationen dar. Linien, Rechtecke, Kreise und Bögen sind
  über ihre stabilen Element-IDs einzeln pickbar; die Auswahl wird farblich und im
  Eigenschaftenbereich angezeigt.
- Der eigenständige Sketch-Editor startet auf einer ausgewählten planaren Fläche und
  erzeugt nacheinander Linien, Rechtecke, Kreise und Mittelpunktbögen im selben temporären Sketch.
  Werkzeugwechsel erhalten bestätigte Elemente; lineares Snapping und Live-Preview
  gelten für alle Zeichenwerkzeuge. Elemente sind bereits während des Editorlaufs über
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
- Beim Öffnen eines Sketches wird die Kamera senkrecht auf die gewählte Arbeitsfläche
  ausgerichtet. Ein eigener Auswahlmodus trennt Selektieren/Dragging von aktiven
  Zeichenwerkzeugen. Sichtbare, zoomunabhängige Punktmarker kennzeichnen Linienenden,
  Kreismittelpunkte sowie Bogenmittel- und Endpunkte. Zeichenwerkzeuge fangen diese
  Referenzen in einem 12-Pixel-Radius und erzeugen beim Bestätigen automatisch eine
  persistente PlaneGCS-Koinzidenz. Während der Bearbeitung liegen Sketch-Geometrie und
  Fangpunkte in einer eigenen Vordergrundebene und bleiben auch auf hellen Flächen
  kontrastreich. Auf Seitenflächen zeigt die globale Z-Richtung nach oben, auf
  horizontalen Flächen die globale Y-Richtung, damit das Modell nicht um 90 Grad kippt.
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
- Zwei Linien beziehungsweise zwei Kreise können mit `Equal` auf gleiche Länge oder
  gleichen Radius gesetzt werden. `Tangent` unterstützt Linie–Kreis und Kreis–Kreis.
  Beide Relationen werden von PlaneGCS gelöst und als `[=]` beziehungsweise `[T]`
  dargestellt.
- `Symmetric` verwendet zwei zuerst gewählte Punkte und eine als dritte Referenz
  gewählte Achslinie. PlaneGCS hält den Mittelpunkt auf der Achse und die Verbindung
  rechtwinklig dazu; die Relation erscheint als `[S]`.
- Zwei ausgewählte Punkte können mit einem treibenden Abstand bemaßt werden; zwei
  ausgewählte Linien mit einem gerichteten Winkel in Grad. Beide Werte sind über
  ihre `D … mm`- beziehungsweise `A …°`-Beschriftung direkt editierbar.
- Gerichtete Punktabstände in X und Y werden als `DX … mm` und `DY … mm` geführt
  und akzeptieren negative Werte sowie null. Jedes Element kann in blaue
  Konstruktionsgeometrie umgeschaltet werden; Constraints bleiben aktiv, Profile und
  Extrusionen ignorieren das Element.
- `Midpoint` bindet einen gewählten Punkt an die Mitte einer Linie. `Perpendicular
  bisector` macht die zuerst gewählte Linie zur parametrischen Mittelsenkrechten der
  zweiten. Beide funktionieren auch mit Konstruktionslinien und erscheinen als `[M]`
  beziehungsweise `[PB]`.
- Zwei Kreise können mit `Concentric` über ihre Mittelpunkte gekoppelt werden, ohne
  ihre Radien gleichzusetzen. Eine treibende Durchmesserbemaßung wird direkt von
  PlaneGCS gelöst und als `Ø … mm` angezeigt und editiert.
- `Collinear` hält zwei Linien auf derselben Geraden. Zwei gewählte Punkte können als
  eigenständige Designabsicht horizontal oder vertikal ausgerichtet werden; die
  Viewport-Glyphen lauten `[CL]`, `[HP]` und `[VP]`.
- Linien und Kreise lassen sich mit einem einzelnen `[F]`-Constraint vollständig
  fixieren. Die Sollkoordinaten bleiben separat gespeichert und widerstehen Dragging.
  Ein Punkt-zu-Linie-Abstand ist als `PL … mm` direkt editierbar.
- Das Mittelpunktbogen-Werkzeug verwendet drei Klicks für Mittelpunkt, Radiuspunkt
  und Endpunkt. PlaneGCS koppelt Radius, Winkel und beide Endpunkte über seine nativen
  Arc-Regeln. Gemischte geschlossene Linien-Bogen-Konturen sind als Profile extrudierbar.
- Polylinien, Dreipunktbögen, Ellipsen, Langlöcher und regelmäßige Polygone ergänzen
  die Zeichenwerkzeuge. Langlöcher und Polygone werden in einzeln bearbeitbare Linien
  und Bögen mit parametrischen Binnenrelationen zerlegt.
- Trimmen, Verlängern, Teilen, Verrunden, Versetzen, Spiegeln sowie lineare und
  kreisförmige Muster arbeiten im temporären Editor. Eine sichtbare Rahmenauswahl,
  schrittweises Escape und lokales Strg-Z/Strg-Y vervollständigen den Bedienablauf.
- Sketch-Ursprung und lokale Achsen sind sichtbare Fangreferenzen. Kanten einer neu
  gewählten planaren Fläche werden violett projiziert; ihre End- und Mittelpunkte
  können neue Geometrie über persistente Koordinatenconstraints binden. Diese
  Constraints speichern Kantenabschnitt und relative Position; nach Änderungen des
  Stützfeatures werden Fläche, Skizzenebene und gebundene Punkte vor dem PlaneGCS-Lauf
  neu aufgelöst.
- Beim Zeichnen werden Koinzidenz, Mittelpunkt, Punkt-auf-Linie, Horizontal,
  Vertikal, Parallel, Rechtwinkligkeit und Linie-Kreis-Tangentialität automatisch
  erkannt, exakt eingerastet und als dauerhafte Relation übernommen. Die Vorschau
  zeigt die bevorstehende Relation bereits vor dem Bestätigen.
- Kreise und Mittelpunktbögen erkennen außerdem gleichen Radius, konzentrische
  Mittelpunkte sowie innere und äußere Berührung mit vorhandenen Kreisen und Bögen.
  Der Radius rastet auf den exakten Solverwert ein; `[=?]`, `[CC?]` und `[T?]`
  machen die Designabsicht vor dem Bestätigen sichtbar.
- Neue Dateien verwenden `VOLUND 16`; bestehende `VOLUND 1`-Rechtecksketches,
  `VOLUND 2`-Linien, `VOLUND 3`-Richtungsconstraints und `VOLUND 4`-Extrusionen
  sowie `VOLUND 5`-Profilauswahlen werden beim Laden migriert. Alte Extrusionen
  behalten dabei ihre bisherige Semantik, alle geschlossenen Profile zu verwenden.
- Ein temporäres generisches Tool-Panel bietet Parameter, Live-Vorschau, OK und
  Cancel. Erst OK schreibt das Feature in das Dokument; Cancel oder Escape verwirft
  die Vorschau.
- Die Sketch-Relationen sind im Tool-Panel in die aufklappbaren Bereiche Linien,
  Punkte, Relationen, Bemaßungen und Bearbeiten gegliedert; dadurch bleibt jeweils
  nur eine kurze, kontextabhängig aktivierte Befehlsgruppe sichtbar.
- Eigenschaften und aktives Werkzeug teilen sich ein kompaktes rechtes Tab-Dock,
  statt den Viewport übereinander zu verengen. Sketch-Werkzeuge sind über eine
  direkt anklickbare Schaltflächenmatrix erreichbar; Relationen, Bemaßungen und
  Bearbeitungsbefehle liegen zweispaltig in kurzen Tabs. Zahlenfelder erscheinen
  nur für das aktive Werkzeug beziehungsweise die aktuelle Auswahl.
- Die neue Völund-Oberflächenvorschau übernimmt die dunkle grün-goldene Gestaltung
  des Völund Vault. Ribbon, Browser, Timeline und Statuszeile bilden ein ruhiges
  Fusion-orientiertes Arbeitsgerüst; der OCCT-Viewport bleibt der echte interaktive
  Viewer und kein statisches Mockup.
- Der Viewer besitzt räumliches Raster, Tiefenwirkung, Achsentriedron sowie einen
  eigenen Kamera-Würfel für Top, Front, Right, Isometric, Fit und den Wechsel zwischen
  orthografischer und perspektivischer Projektion. Orbit, Pan und Zoom bleiben direkt
  im Viewport bedienbar.
- Beim Programmstart ist kein Modellierwerkzeug voraktiviert. Temporäre
  Werkzeugparameter werden nur während des aktiven Befehls eingeblendet; OK und
  Cancel beenden den Befehl und entfernen die Palette wieder.
- Der Kamera-Würfel wird ohne rechteckige Trägerfläche transparent über dem
  3D-Viewport komponiert. Windows verwendet deshalb wie FreeBSD standardmäßig den
  Qt-eigenen OpenGL-Kontext. Die dafür benötigten Qt-OpenGL-Laufzeitbibliotheken
  werden beim Windows-Build automatisch neben der Vorschau-Anwendung bereitgestellt.
- Der Sketch-Befehl der neuen Oberfläche verwendet keine separate 2D-Demofläche
  mehr. Er bleibt im echten OCCT-Viewport, richtet die ausgewählte planare Fläche
  senkrecht aus und verbindet Linie, Rechteck, Kreis und Mittelpunktbogen direkt mit
  der parametrischen `SketchEditorSession`. Live-Vorschau, Fangreferenzen,
  automatische Grundrelationen, Auswahl, Dragging, Freiheitsgrade und sichtbare
  Constraint-Glyphen werden aus demselben Editorzustand dargestellt. Der UI-Smoke-Test
  durchläuft die direkt sichtbaren Grundtypen vor dem Commit des Sketch-Features.
- Die Create-Gruppe stellt zusätzlich Polylinie, Dreipunktbogen, Ellipse, Langloch
  und regelmäßiges Polygon über kompakte Dropdowns der zugehörigen Grundwerkzeuge
  bereit. Damit bleiben neun parametrische Geometrietypen erreichbar, ohne das Ribbon
  mit Einzelbuttons zu überladen. Der UI-Smoke-Test erzeugt jeden Typ über dieselbe
  Preview-Brücke; Polylinien führen den nächsten Abschnitt am letzten Endpunkt fort.

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
- Sechzehn CTest-Tests prüfen zusätzlich den unveränderten PlaneGCS-Kern sowie den
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
sowie Tangential-, Gleichheits- und Symmetrierelationen sind umgesetzt. Mittelpunkt,
Mittelsenkrechte, konzentrische Kreise, Durchmesserbemaßung, Linienflucht und
Punktausrichtungen sowie Fixierung und Punkt-Linien-Abstand sind vollständig parametrisch.

Der direkte Push/Pull-Ablauf bleibt dabei als schneller Weg erhalten. Er ist eine
Bedienoberfläche über denselben parametrischen Sketch- und Extrusionsfeatures, kein
Ersatz für den vollständigen Sketch-Editor.

Alle grundlegenden Sketch-Erzeugungswerkzeuge besitzen nun eine durchgängige
Live-Vorschau. Linie, Polylinie, Rechteck, Kreis und Polygon zeigen unmittelbar ihre
Kontur. Bei Mittelpunktbogen, Dreipunktbogen, Ellipse und Slot wird in der ersten
Eingabephase zunächst Radius, Sehne beziehungsweise Achse dargestellt und nach dem
zweiten Klick die vollständige Kontur bis zur Bestätigung aktualisiert. Unvollständige
Zwischengeometrie bleibt reine Vorschau und kann nicht in den Sketch übernommen werden.
Ein Integrationstest prüft für alle neun Werkzeugtypen, dass jede Vorschau eine
renderbare OCCT-Geometrie erzeugt.

## Übergabe für den nächsten Arbeitstag

Der visuelle Grundaufbau und die Viewer-Navigation sind abgenommen. Der nächste Slice
beginnt nicht mit einer weiteren flächigen Umgestaltung, sondern bindet die
Modellierbefehle schrittweise an das neue Bedienkonzept:

1. Sketch als erster vollständiger Ribbon-zu-Viewport-zu-Toolpanel-Ablauf;
2. kontextabhängige Sketchauswahl und saubere Bearbeitungsebene;
3. Extrude/Push-Pull mit echter Profilwahl, Live-Vorschau, OK und Cancel;
4. anschließend Fillet und weitere Solid-Werkzeuge nach demselben Lebenszyklus.

Bekannt und bewusst noch offen sind die endgültige grafische Ausarbeitung des
Kamera-Würfels, die vollständige funktionale Ribbon-Belegung und die Meridian-Abnahme
auf dem Ziel-Desktop. Zwei irrtümlich beim SCP-Abgleich erzeugte Kopien
`/home/eduard/src/voelund/PreviewWindow.cpp` und
`/home/eduard/src/voelund/PreviewStyle.cpp` liegen auf der FreeBSD-Maschine außerhalb
des Buildpfads; sie wurden mangels ausdrücklicher Löschfreigabe nicht entfernt.

## Entwicklungsbasis ab 13.09.2026

Gemäß [ADR 0007](adr/0007-sway-development-meridian-final.md) wird das CAD-Tool auf FreeBSD/Sway stabilisiert. Meridian folgt abschließend in L07 und blockiert keine CAD-Erweiterungen. [P04](implementation-plan/evidence/P04.md) ist mit 23/23 CTests und betrachteten Compositoraufnahmen als Sway-Basis abgenommen; A02/A04 und die dokumentierten Grafik-/Hardware-Restprüfungen bleiben offen.
