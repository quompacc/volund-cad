# Völund CAD – Handbuch (mechanisches v1, Stand 30.10.2026)

Dieses Handbuch beschreibt, was Völund CAD heute kann und wo die Grenzen liegen. Es nennt nur
umgesetzte Funktionen. Was hier nicht steht, gibt es noch nicht.

Neu bei Völund? Das [Tutorial „Referenzplatte“](tutorial-referenzplatte.md) führt in etwa
15 Minuten durch ein erstes Teil.

## Arbeitsweise

- **Fenster:** Die 3D-Ansicht füllt das Fenster. Alles andere liegt als Karte darüber:
  - oben links Dokument und Dateimenü;
  - links Modellbaum und die Werkzeugleiste mit Gruppen (Erstellen, Skizze, Bearbeiten,
    Bewegen, Mehr);
  - oben rechts Suche und Rückgängig/Wiederholen;
  - rechts Ansichtswürfel und Werkzeug-Panel;
  - unten Mitte die Timeline.

  Am gewählten Objekt erscheint eine Kontextleiste.
- **Ein Werkzeug zur Zeit:** Jedes Werkzeug öffnet ein Panel mit Vorschau. Der wichtigste Wert
  hat gleich den Fokus. OK, Enter oder Strg+Enter übernimmt alles als einen Rückgängig-Schritt;
  Abbrechen lässt das Dokument unverändert.
- **Fehler:** Geht eine Änderung nicht auf, bleibt das Panel offen und nennt den Grund.
  - Betrifft der Fehler nur spätere Features, bietet das Panel „Mit Folgefehlern übernehmen“ an.
  - Diese Features bleiben dann mit Fehler stehen, bis sie repariert werden. Gelöscht wird dabei
    nie etwas.
- **Dokumente:** Neu (Strg+N), Öffnen (Strg+O), Speichern (Strg+S), Speichern unter,
  Duplizieren.
  - Das Untermenü **Zuletzt geöffnet** zeigt die letzten zehn Dokumente. Eine fehlende Datei wird
    genannt und aus der Liste genommen; „Liste leeren“ leert sie.
  - Bei ungespeicherten Änderungen fragt Völund vorher.
  - Dateien haben die Endung `.volund`.
  - Ungespeicherte Arbeit wird regelmäßig gesichert und nach einem Absturz zur Wiederherstellung
    angeboten.
- **Auswahl:**
  - Strg-Klick nimmt hinzu oder heraus, Umschalt-Klick wählt einen Bereich. Das gilt im
    Modellbaum, in der Timeline und in der Ansicht.
  - Aus dem Leeren ziehen wählt Körper: nach rechts die ganz umschlossenen, nach links alle
    berührten. Eine leere Box hebt die Auswahl auf.
  - Ein Doppelklick auf eine Kante wählt ihre Tangentialkette. Ein erneuter Klick an derselben
    Stelle wechselt zwischen Kante, Fläche dahinter und den nächsten Elementen.
  - Der **Auswahlfilter** (Knopf links neben „◐“) schaltet Körper, Flächen, Kanten, Ebenen und
    Achsen sowie Skizzenflächen einzeln ab. Ein Punkt am Knopf zeigt, dass etwas aus ist.
- **Tasten wie in Fusion:**
  - im Modell: E Extrudieren, Q Drücken/Ziehen (bei Kanten Verrunden, bei einem Skizzenbereich
    Extrudieren), F Verrunden, H Bohrung, M Bewegen, I Messen, V Ein-/Ausblenden, Entf Löschen,
    F6 Einpassen, Umschalt+R wiederholt den letzten Werkzeugbefehl;
  - in der Skizze: L Linie, R Rechteck, Umschalt+R Rechteck (Mitte), C Kreis, A Bogen,
    Umschalt+A Dreipunktbogen, D Bemaßung, X Hilfsgeometrie, T Stutzen.
- **Kopieren und Einfügen:** Strg+C (auch in der Kontextleiste eines Körpers) und Strg+V legen
  eine feste, unabhängige Kopie als eigenen Schritt „Kopie N“ an, auch in einem anderen
  Dokument. Spätere Änderungen am Original wirken nicht auf die Kopie.
- **Gruppen:** Löschen, Unterdrücken, Ausblenden, Kopieren und Bewegen wirken auf alle gewählten
  Körper oder Schritte als ein Rückgängig-Schritt.

## Skizzen

- Skizzen liegen auf einer Grundebene (XY, XZ, YZ), einer Bezugsebene oder einer ebenen
  Körperfläche. Auf einer Fläche wandert die Skizze mit, wenn sich die Fläche verschiebt.
- **Elemente:**
  - Linie, Polylinie;
  - Rechteck (Ecke, Mitte oder drei Punkte), auf Wunsch mit „Abgerundete Ecken (U)“ oder
    „Rahmen (J)“;
  - Vieleck, Langloch;
  - Kreis (Mitte, zwei oder drei Punkte), Bogen (Mittelpunkt, drei Punkte, tangential), Ellipse;
  - Punkt;
  - Spline über Kontrollpunkte (B-Spline) oder **durch Punkte**: Jeder Klick setzt einen Punkt,
    durch den der Spline läuft; Enter beendet.
  - **Text:** Ein Dialog fragt Text, Höhe der Großbuchstaben und Schrift (die mitgelieferte
    Albert Sans oder eine Systemschrift). Der Text beginnt am gewählten Punkt, sonst am
    Ursprung. Er wird sofort zu Skizzenkurven und lässt sich gravieren oder prägen (Bereiche der
    Buchstaben extrudieren), aber nicht mehr als Text ändern.
- **Werte direkt eingeben:** Nach dem ersten Punkt hängen Felder an der Maus, auf den Maßlinien
  der Vorschau:
  - Linie: Länge und Winkel; Rechteck: Breite und Höhe; Kreis: Durchmesser;
  - Bogen: Radius, dann Öffnungswinkel; Langloch: Länge, dann Breite; Ellipse: Radius, dann
    kleiner Radius; Vieleck: Radius.

  Einfach tippen, ohne zu klicken: **Tab** wechselt das Feld, **Enter** setzt das Element. Ein
  getippter Wert ist fest, die anderen folgen der Maus. Getippte Werte der ersten Stufe werden
  Maße der Skizze (mit dem Element ein Rückgängig-Schritt); Werte der zweiten Stufe (z. B. der
  Öffnungswinkel) legen nur die Form fest. „=Name“ bindet das Maß an einen Parameter, „=B*2“
  rechnet.
- **Bemaßen (D):** Linie anklicken zeigt ihre Länge, Kreis den Durchmesser, Bogen den Radius, in
  einem Feld an der Maus: Wert tippen, Enter. Ein zweites Element vor dem Tippen macht daraus
  Winkel oder Abstand. Esc behält den gemessenen Wert.
  - Ein Doppelklick auf ein Maß ändert es im selben Feld.
  - Die Maßzahl lässt sich ziehen und verschiebt die Maßlinie.
  - Maße erscheinen wie in einer Zeichnung („80 mm“, „R 10 mm“, „Ø 10 mm“, „90°“); gebundene
    als „Breite = 80 mm“, Referenzmaße grau in Klammern.
  - Ein Maß, das die Skizze überbestimmen würde, wird abgelehnt.
- **Beziehungen:** Deckungsgleich, horizontal, vertikal, parallel, senkrecht, tangential,
  gleich, konzentrisch, fixiert, Mittelpunkt, symmetrisch, Punkt auf Objekt. Sie erscheinen als
  Symbolkästchen; ein Klick wählt, Entf löscht.
- **Skizzen-Karte:** Ein Balken zeigt, wie weit die Skizze bestimmt ist. Die Karte nennt offene
  Enden und nur aufeinandergelegte Ecken („Verbinden“ verbindet sie) und listet Beziehungen und
  Elemente.
- **Bearbeiten:**
  - **Stutzen, Verlängern, Teilen** von Linien, Bögen, Kreisen (nur Stutzen), Ellipsen (nur
    Stutzen, sie werden zu Ellipsenbögen), Ellipsenbögen und Splines (Stutzen und Teilen,
    exakt). Ellipsen, Ellipsenbögen und Splines schneiden dabei mit.
  - **Versetzen** von Linien, Bögen, Kreisen und Konturen. Eine Ellipse, ein Ellipsenbogen
    oder ein Spline wird einzeln zu einem Spline in genau dem Abstand (auf 1 µm); eine ganze
    Ellipse wächst nach außen.
  - **Verrunden und Fasen** zwischen zwei Linien, zwischen Linie und Bogen und zwischen zwei
    Bögen, die sich an einem Endpunkt treffen.
  - **Spiegeln**, **lineares und kreisförmiges Muster**. Mit „Musterbezug: Zuletzt gewähltes
    Element“ läuft das Muster entlang einer gewählten Linie oder dreht um einen gewählten Punkt,
    Kreis oder Bogen.
  - **Drehen** und **Skalieren** der Auswahl.
- **Kurven von außen:**
  - **Projizieren:** Körperkanten werden als Hilfsgeometrie projiziert und folgen ihrem Körper.
  - **Schnittkurven:** Wo sichtbare Körper die Skizzenebene schneiden, entstehen gewöhnliche
    Skizzenkurven, in einem Schritt.
  - **DXF importieren:** Linien, Bögen, Kreise, Polylinien (auch mit Bögen), Punkte, Ellipsen
    und Splines einer DXF-Datei kommen in die offene Skizze, in mm nach der Einheit der Datei.
    Die Ebene „Konstruktion“ wird Hilfsgeometrie. Blöcke, Text und Schraffuren werden
    übersprungen; die Statuszeile nennt sie.
- **Beenden:** OK übernimmt. Ist nichts mehr zurückzunehmen, beendet ein zweites Esc die Skizze
  wie OK. Verwerfen geht nur über Abbrechen mit Rückfrage. Ein Rechtsklick öffnet das
  Skizzenmenü mit „Skizze beenden“.
- **Grenzen:** Splines lassen sich nicht verlängern. Eine Ellipse geht nicht in Verrundungen
  und Fasen. Nach dem Teilen hält keine Beziehung die Stücke einer Ellipse auf derselben
  Ellipse. Text bleibt nicht als Text bearbeitbar. Einige technische Meldungen der
  Bereichsauswertung sind englisch.

## Körper erstellen

- **Extrudieren (E)** von Skizzenbereichen. Die Bereiche der gewählten Skizze sind gefüllt; ein
  Klick wählt, Strg ergänzt.
  - Ergebnis als neuer Körper, vereinigt, abgezogen (Tasche) oder geschnitten.
  - Richtung umkehrbar, symmetrisch (Länge = Gesamtlänge), Startversatz, bis zu einer ebenen,
    parallelen Fläche.
  - **Schrägungswinkel** von −88,9° bis 88,9° (positiv wird das Profil breiter; nicht bei
    symmetrisch).
  - Die Länge kann sofort getippt werden. Ein Pfeil auf dem Profil lässt sich ziehen; durch das
    Profil hindurch kehrt er die Richtung um.
  - Vereinigende Extrusionen hinterlassen keine Naht. Der Schalter „Naht entfernen“ in den
    Eigenschaften schaltet das je Schritt um.
- **Drücken/Ziehen (Q)** einer ebenen Körperfläche, ebenfalls ohne Naht.
- **Drehen** (Revolve) und **Nut** um eine Skizzenlinie, Grund- oder Bezugsachse; „Ende: Bis
  Fläche“ dreht bis zu einer ebenen Fläche, die die Achse enthält.
- **Folgen** (Sweep) entlang einer Pfadskizze, **Ausformung** (Loft) zwischen zwei Profilen,
  **Wendel** (Helix).
- **Rippe:** aus genau einer geraden Linie der gewählten Skizze, mit Dicke (symmetrisch zur
  Skizzenebene) und Richtung, bis zum Körper.
- **Grundkörper:** Quader, Zylinder, Kugel, Kegel, Torus, Keil – als eigener Körper oder mit
  einem Körper vereinigt, abgezogen oder geschnitten.
- **Bezugsgeometrie** (unter „Mehr“): Bezugsebene, Bezugsachse, **Mittelebene** zwischen zwei
  parallelen Flächen, **Ebene durch drei Bezugspunkte**, **Bezugspunkt** an X/Y/Z oder auf einer
  Kante (Anfang, Ende, Mitte, Kreismittelpunkt). Bezugselemente folgen ihren Körpern und haben im
  Modellbaum ein Auge.
- **Grenzen:** Keine Führungskurven bei Folgen und Ausformung. Keine kegelige Wendel. Eine Rippe
  braucht eine gerade Linie.

## Körper ändern

- **Verrunden (F)** und **Fasen** von Kanten. Kanten werden angeklickt, Strg fügt weitere
  hinzu, eine Box im Werkzeug nimmt auch verdeckte. Fasenarten: gleiche Abstände, zwei Abstände,
  Abstand und Winkel; „Richtung umkehren“ tauscht die Flächen.
- **Bohrung (H)** auf einer ebenen Fläche:
  - Lage per Klick und X/Y im Rahmen der Fläche oder „An zwei Kanten“ mit getippten Abständen.
    Der Rahmen einer Fläche zeigt nicht immer in die Richtungen des Modells (auf der Oberseite
    einer Platte kann X nach links zeigen); „An zwei Kanten“ ist eindeutig.
  - Art: Einfach, Flachsenkung oder Senkung; Spitze flach oder 118°; Tiefe oder durchgehend.
  - Gewinde metrisch ISO M1,6 bis M24 als Angabe; modelliert wird das Kernloch.
  - Ist eine Skizze mit Punkten auf einer Körperfläche gewählt, setzt „Bohrung“ an jedem Punkt
    eine gleiche Bohrung (höchstens 100) in einem Schritt.
- **Wandstärke** (Schale) mit offenen Flächen. **Formschräge** an ebenen Flächen.
- **Fläche versetzen** und **Fläche löschen** (Bearbeiten): Flächen um einen Abstand verschieben
  oder entfernen, auch gekrümmte. Eine Bohrungswand ändert so den Durchmesser, eine Ebene nimmt
  tangentiale Verrundungen mit. Beim Löschen schließen die Nachbarflächen die Lücke (eine Fase heilt
  zur Kante, eine Bohrung verschwindet). Geht das nicht, wird es mit Begründung abgelehnt.
- **Teilen** an einer Grundebene, Konstruktionsebene oder ebenen Fläche; beide Seiten oder nur
  eine behalten.
- **Spiegeln** an einer Ebene oder ebenen Fläche, **Muster** linear, kreisförmig, als **Raster**
  oder **entlang einer Kurve** – als neue Körper oder vereinigt. Einzelne Kopien lassen sich
  überspringen. Sind vorher Schritte in der Timeline gewählt (Bohrungen, vereinigende oder
  abziehende Extrusionen), wiederholen Muster und Spiegeln diese Schritte statt des Körpers.
- **Kombinieren** ganzer Körper (vereinigen, abziehen, schneiden).
- **Bewegen (M)** und Drehen mit dem Manipulator oder über Werte. Nach dem Ziehen steht der Wert
  im Feld an der Maus; eine Zahl und Enter machen ihn genau, Esc behält ihn. Für Körper, die
  nicht ihr eigener Grundkörper sind, entsteht ein Schritt „Bewegen N“.
- **Körper** lassen sich im Baum umbenennen und ausblenden.
- **Grenzen:** Kein variabler Radius. Kein modelliertes Gewinde. Wandstärke nur mit einer
  Dicke. Formschräge ohne Trennlinie. Nur ebene Flächen lassen sich versetzen oder löschen.
  Gruppen lassen sich verschieben, nicht drehen. Spätere Schritte können Flächen von
  Feature-Kopien eines Musters noch nicht nutzen.

## Historie

- Die **Timeline** zeigt jedes Feature als Schritt. Ein Klick (oder Enter) öffnet es zum
  Bearbeiten; dabei wird vorübergehend bis zu ihm zurückgerollt (nicht bei Skizzen). Pfeiltasten,
  Pos1 und Ende wählen Schritte.
- Das **Kontextmenü** eines Schritts: Bearbeiten, Unterdrücken, Zurückrollen bis hier, Ans Ende,
  Löschen.
- **Unterdrücken/Aktivieren:** Ein unterdrücktes Feature wird übersprungen; spätere Features
  arbeiten mit dem Körper ohne es.
- **Zurückrollen:** Die Marke in der Timeline lässt sich ziehen oder per Menü setzen. Das Modell
  zeigt dann den Stand an der Marke, neue Features entstehen dort. Zurückrollen ist kein
  Rückgängig-Schritt und keine ungesicherte Änderung.
- **Umordnen:** Einen Schritt an eine andere Stelle ziehen. Vor seine eigenen Eingänge (z. B.
  eine Extrusion vor ihre Skizze) wird er nicht gelassen.
- **Löschen** (Entf, Kontextleiste, Menü): Ohne Abhängige sofort. Mit Abhängigen fragt ein
  Dialog: mit allen Abhängigen löschen, nur dieses Feature löschen (die Abhängigen bleiben mit
  Fehler zum Reparieren) oder abbrechen. Mehrere gewählte Schritte gehen zusammen.
- **Reparatur:** Fehlende Bezüge einer Skizze oder Extrusion werden angezeigt und lassen sich
  bewusst neu zuordnen.
- **Grenzen:** Umordnen immer nur für einen Schritt.

## Parameter

- Die **Parametertabelle** ist eine Karte unter dem Modellbaum (Knopf „Parameter“ oder Strg+P).
  Sie zeigt Name, Wert und Verwendungen; Name und Wert ändert man direkt in der Zeile. „+ Länge“
  und „+ Winkel“ legen einen Parameter an.
- **Ausdrücke:** Ein Wert darf ein Ausdruck sein, auch in der Tabelle („=A*2 + 4“): + − × ÷ ^,
  Klammern, Einheiten (mm, cm, m, in, °, deg, rad), Funktionen (sqrt, sin, cos, tan, abs, min,
  max, round) und pi. Ein Zyklus wird mit Pfad abgewiesen.
- **Binden:** Statt einer Zahl `=A` oder `=Ausdruck` eingeben, mit Vorschlägen beim Tippen. Das
  geht in allen Zahlenfeldern der Werkzeuge (Extrusion, Bohrung, Verrunden, Fase, Muster auch
  in der Anzahl, Wandstärke, Formschräge, Bezugsebene, Bewegen, Drücken/Ziehen, Drehen,
  Wendelsteigung, Grundkörper, Fläche versetzen) und in Skizzenmaßen.
  - Ein unbekannter Name oder ein Ausdruck legt nach kurzer Frage einen Parameter an.
  - Gebundene Felder zeigen „Name = Wert“. Eine Zahl im Feld löst die Bindung.
- **Ändern:** Ändert man einen Wert in der Tabelle, rechnet das ganze Modell neu, als ein
  Rückgängig-Schritt. Umbenennen schreibt die Ausdrücke um.
- **Löschen:** Einen benutzten Parameter löscht Völund nur nach Rückfrage; die Felder behalten
  ihren Wert. Solange ein Ausdruck ihn benutzt, ist Löschen gesperrt.
- **Grenzen:** Nicht bindbar sind die Drehung eines Grundkörpers, die Höhe einer Wendel, der
  Schrägungswinkel und Skizzenfelder außer Linienlänge und Radius.

## Messen und Prüfen

- **Messen (I)** mit zwei Klicks: Länge, Radius, Fläche, Volumen eines Elements; Abstand und
  Winkel zwischen zwei Elementen. Eckpunkte sind anklickbar; „Kanten messen als“ nimmt die
  Kante, ihren Mittelpunkt oder ihr Kreiszentrum. Ein Punkt zeigt seine Lage, zwei Punkte
  Abstand und Δ je Achse. Drei Nachkommastellen, Kopieren mit voller Genauigkeit.
- **Überschneidungen** (unter „Mehr“): prüft die gewählten Körper, sonst alle sichtbaren, und
  nennt je Paar das gemeinsame Volumen; die Überschneidung ist rot umrandet.
- **Material und Masse:** Die Körperkarte bietet Materialien (Stahl, Edelstahl, Gusseisen,
  Aluminium, Messing, Kupfer, Titan, PLA, ABS, PETG, Nylon, Buche) oder „Eigene Dichte“
  (0,001–30 g/cm³). Darunter stehen Masse, Volumen, Oberfläche, Schwerpunkt und
  Trägheitsmomente; bei mehreren Körpern die Summe. Ohne Material steht „Masse: – (ohne
  Material)“.
- Messen und Prüfen verändern das Dokument nicht.
- **Grenzen:** Keine Toleranzprüfung, keine Festigkeitswerte.

## Schnittansicht

- **Schnitt** (Symbol in der Kopfzeile, Befehlssuche) schneidet die Körper an einer Ebene auf.
  Die Karte unten links wählt XY, XZ, YZ, eine Bezugsebene oder mit „Auswahl“ die gewählte ebene
  Fläche. Der Versatz verschiebt den Schnitt, im Feld oder mit dem Pfeil im Ansichtsfenster.
  „Seite umkehren“ zeigt die andere Hälfte.
- Die Schnittfläche ist dunkler gefüllt. Auswahl und Messen arbeiten am geschnittenen Modell.
- Der Schnitt bleibt beim Arbeiten mit anderen Werkzeugen stehen. ✕ oder ein zweiter Klick auf
  „Schnitt“ zeigt wieder das ganze Modell. Der Schnitt ändert das Dokument nicht.
- **Grenzen:** Ein Schnitt zur Zeit; die Schnittfläche selbst ist nicht wähl- oder messbar.

## Darstellung und Farben

- Der Knopf **◐** neben dem Ansichtswürfel wählt die Darstellung: schattiert, schattiert mit
  Kanten, Drahtgitter oder verdeckte Kanten (gestrichelt). Die Wahl gilt für diesen Rechner.
- Ist eine Körperzeile im Modellbaum gewählt, zeigt rechts eine Karte die **Farbe** des Körpers:
  zehn Felder, „Eigene…“ oder „Standard“. Die Farbe wird mit dem Dokument gespeichert und ist ein
  Rückgängig-Schritt. STEP und IGES schreiben sie mit; ein STEP-Import übernimmt sie.
- **Grenzen:** Keine Texturen und Renderings; eine Farbe je Körper. STL, 3MF und eingefügte
  Kopien tragen keine Farbe.

## Austausch

- Eine STEP-Datei lässt sich auch einfach ins Fenster ziehen; sie kommt ins aktuelle Dokument. Große Dateien zeigen ihren Fortschritt und lassen sich abbrechen.
- **Importieren (STEP)…** (Datei-Menü, Strg+I) fügt die Volumenkörper einer STEP-Datei als
  einen Schritt „Import N“ ins offene Dokument ein, ein Rückgängig-Schritt.
  - Jeder Volumenkörper wird ein eigener Körper mit dem Namen und der Farbe aus der Datei.
  - Dateien in Zoll, Meter usw. werden in mm umgerechnet; die Statuszeile nennt die Einheit.
  - Die Körper sind danach gewöhnliche Körper: verrunden, bohren, bewegen usw.
  - **Öffnen** nimmt auch `.step`/`.stp` und legt dafür ein neues Dokument an.
  - Die Volumenkörper werden im Dokument gespeichert; die STEP-Datei wird danach nicht mehr
    gebraucht.
  - Flächen, Schalen oder Kurven ohne Volumen werden nicht übernommen; ein Hinweis nennt ihre
    Zahl. Eine unlesbare Datei meldet einen Fehler und ändert nichts.
- **Exportieren…** (Strg+E) schreibt den im Baum gewählten Körper, sonst alle sichtbaren Körper,
  in mm mit ihren Namen. Das Format folgt dem gewählten Dateityp:
  - STEP und IGES schreiben die exakten Körper.
  - „DXF (Skizze)“ schreibt die gewählte Skizze in ihren eigenen Koordinaten; Hilfsgeometrie
    liegt auf dem Layer „Konstruktion“.
  - STL und 3MF schreiben Dreiecksnetze für den 3D-Druck. Vorher fragt Völund die Auflösung:
    grob (0,1 mm), mittel (0,025 mm) oder fein (0,005 mm). STL enthält alle Dreiecke, 3MF ein
    Objekt je Körper.
  - Hat das Modell Fehler, wird der Export abgelehnt – auch wenn noch eine ältere gültige Form
    angezeigt wird.
- **DXF in eine Skizze:** siehe „Kurven von außen“ bei den Skizzen.
- **Grenzen:** Aus STEP entsteht keine parametrische Historie. Baugruppenstruktur, Ebenen und
  PMI werden nicht übernommen. Kanten importierter Körper lassen sich nicht in Skizzen
  projizieren oder als Achse nutzen (Verrunden und Fasen gehen). Ein Import darf höchstens
  10.000 Volumenkörper und 960 MiB Geometrie enthalten; bei sehr vielen Körpern dauert jede Änderung aber lange
  (gemessen mit 1667 Körpern: rund drei Minuten). Kein IGES- und kein Netz-Import. Ein streng prüfendes AutoCAD kann
  die DXF-Datei des Exports ablehnen.

## Weitere Grenzen von v1

- Keine Baugruppen, keine Zeichnungen, keine Blechfunktionen, keine Freiformflächen.
- Völund läuft auf Windows und Linux; FreeBSD und macOS folgen später.
- Technische Kernelmeldungen sind teils englisch; die häufigen Fälle sind deutsch erklärt.
