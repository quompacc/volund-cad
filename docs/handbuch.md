# Völund CAD – Handbuch (mechanisches v1, Stand 16.10.2026)

Dieses Handbuch beschreibt, was Völund CAD heute kann und wo die Grenzen liegen. Es nennt nur
umgesetzte Funktionen. Was hier nicht steht, gibt es noch nicht.

## Arbeitsweise

- **Fenster:** Die 3D-Ansicht füllt das Fenster; alles andere liegt als Karte darüber: oben
  links Dokument und Dateimenü, links Modellbaum und die Werkzeugleiste mit Gruppen
  (Erstellen, Skizze, Bearbeiten, Bewegen, Mehr), oben rechts Suche und
  Rückgängig/Wiederholen, rechts Ansichtswürfel und Werkzeug-Panel, unten Mitte die
  Timeline. Am gewählten Objekt erscheint eine Kontextleiste.
- **Ein Werkzeug zur Zeit:** Jedes Werkzeug öffnet ein Panel mit Vorschau. OK (oder Strg+Enter)
  übernimmt alles als einen Rückgängig-Schritt, Abbrechen lässt das Dokument unverändert.
- **Fehler:** Geht eine Änderung nicht auf, bleibt das Panel offen und nennt den Grund. Betrifft
  der Fehler nur spätere Features, bietet das Panel „Mit Folgefehlern übernehmen“ an. Diese
  Features bleiben dann mit Fehler stehen, bis sie repariert werden. Gelöscht wird dabei nie
  etwas.
- **Dokumente:** Neu (Strg+N), Öffnen (Strg+O), Speichern (Strg+S), Speichern unter,
  Duplizieren. Bei ungespeicherten Änderungen fragt Völund vorher. Dateien haben die Endung
  `.volund`. Ungespeicherte Arbeit wird regelmäßig gesichert und nach einem Absturz zur
  Wiederherstellung angeboten.

## Skizzen

- Skizzen liegen auf einer Grundebene (XY, XZ, YZ), einer Bezugsebene oder einer ebenen
  Körperfläche. Auf einer Fläche wandert die Skizze mit, wenn sich die Fläche verschiebt.
- **Elemente:** Linie, Polylinie, Rechteck (Ecke oder Mitte), Vieleck, Kreis, Bogen
  (Mittelpunkt oder drei Punkte), Ellipse, Langloch, Punkt, B-Spline über Kontrollpunkte,
  projizierte Körperkanten.
- **Beziehungen und Maße:** Deckungsgleich, horizontal, vertikal, parallel, senkrecht,
  tangential, gleich, konzentrisch, fixiert; Länge, Abstand, Radius, Durchmesser, Winkel.
  Maße sind treibend oder Referenzmaße (in Klammern).
- **Bearbeiten:** Stutzen, Verlängern, Teilen, Versetzen, Spiegeln, lineares und kreisförmiges
  Muster, Skizzenverrundung und -fase, Drehen und Skalieren.
- **Grenzen:** Ellipsen lassen sich nicht stutzen, verlängern oder versetzen. Verrundung und Fase
  gehen nur zwischen zwei Linien. Muster laufen nur entlang +X bzw. um den Skizzenursprung.
  Einige technische Meldungen der Bereichsauswertung sind englisch.

## Körper erstellen

- **Extrusion** von Skizzenbereichen. Ergebnis als neuer Körper, vereinigt, abgezogen (Tasche)
  oder geschnitten. Richtung umkehrbar, symmetrisch (Länge = Gesamtlänge), Startversatz, bis
  zu einer ebenen, parallelen Fläche. Bereiche werden durch Anklicken an- und abgewählt.
- **Drücken/Ziehen** einer ebenen Körperfläche. Ein Zug, der eine Fläche des Körpers eben
  fortsetzt, hinterlässt keine Naht (der Quader mit gezogener Oberseite hat glatte Seiten); im
  Eigenschaften-Panel lässt sich das pro Zug abschalten („Naht entfernen“). Züge aus älteren
  Dateien behalten ihre Nähte, bis man es einschaltet.
- **Drehen** (Revolve) und **Nut** um eine Skizzenlinie, Grund- oder Bezugsachse.
- **Folgen** (Sweep) entlang einer Pfadskizze, **Ausformung** (Loft) zwischen zwei Profilen,
  **Wendel** (Helix).
- **Grundkörper:** Quader, Zylinder, Kugel, Kegel, Torus, Keil – als eigener Körper oder mit
  einem Körper vereinigt, abgezogen oder geschnitten.
- **Bezugsebene** und **Bezugsachse**.
- **Grenzen:** Keine Konizität bei der Extrusion. Kein „bis Fläche“ bei Drehen. Keine
  Führungskurven bei Folgen und Ausformung. Keine kegelige Wendel und kein Normgewinde.

## Körper ändern

- **Verrunden** und **Fasen** (symmetrischer Abstand) von Kanten. Kanten werden angeklickt,
  Strg fügt weitere hinzu. Auch Kanten an früheren Rundungen und Fasen lassen sich bearbeiten.
- **Bohrung** auf einer ebenen Fläche: Lage per Klick und X/Y im Flächenrahmen, Durchmesser,
  Tiefe oder durchgehend.
- **Wandstärke** (Schale) mit offenen Flächen. **Formschräge** an ebenen Flächen.
- **Spiegeln** an einer Ebene oder ebenen Fläche, **Muster** linear oder kreisförmig – jeweils
  als neue Körper oder vereinigt.
- **Kombinieren** ganzer Körper (vereinigen, abziehen, schneiden).
- **Bewegen** und Drehen jedes Körpers mit dem Manipulator oder über Werte. Für Körper, die
  nicht ihr eigener Grundkörper sind, entsteht ein Schritt „Bewegen N“.
- **Körper** lassen sich im Baum umbenennen und ausblenden.
- **Grenzen:** Kein variabler Radius, keine asymmetrische oder Winkelfase. Bohrung ohne
  Gewinde, Senkung oder Bohrspitze. Wandstärke nur mit einer Dicke. Formschräge ohne
  Trennlinie. Muster nur von Körpern, nicht von Features; kein Raster und kein Muster entlang
  einer Kurve. Nähte verschwinden nur beim Drücken/Ziehen, nicht beim Extrudieren aus Skizzen.

## Historie

- Die **Timeline** zeigt jedes Feature als Schritt. Ein Klick (oder Enter) öffnet es zum
  Bearbeiten. Pfeiltasten, Pos1 und Ende wählen Schritte.
- **Unterdrücken/Aktivieren:** Ein unterdrücktes Feature wird übersprungen; spätere Features
  arbeiten mit dem Körper ohne es.
- **Zurückrollen:** Die Marke in der Timeline lässt sich ziehen oder per Menü setzen. Das Modell
  zeigt dann den Stand an der Marke. Neue Features entstehen an der Marke. Zurückrollen ist kein
  Rückgängig-Schritt und keine ungesicherte Änderung.
- **Umordnen:** Einen Schritt an eine andere Stelle ziehen. Vor seine eigenen Eingänge (z. B.
  eine Extrusion vor ihre Skizze) wird er nicht gelassen.
- **Löschen** (Entf in der Timeline, Kontextleiste, Menü): Ohne Abhängige sofort. Mit Abhängigen
  fragt ein Dialog nach dem Weg: mit allen Abhängigen löschen, nur dieses Feature löschen (die
  Abhängigen bleiben mit Fehler zum Reparieren) oder abbrechen.
- **Reparatur:** Fehlende Bezüge einer Skizze oder Extrusion werden angezeigt und lassen sich
  bewusst neu zuordnen.
- **Grenzen:** Umordnen und Löschen immer nur für einen Schritt.

## Parameter

- Die **Parametertabelle** ist eine Karte unter dem Modellbaum. Man öffnet sie mit dem Knopf
  „Parameter“ in der Kopfzeile oder mit Strg+P.
  - Sie zeigt Name, Wert und wie oft ein Parameter verwendet wird.
  - Name und Wert ändert man direkt in der Zeile.
  - „+ Länge“ und „+ Winkel“ legen einen Parameter an.
- **Binden:** Statt einer Zahl `=A` eingeben, mit Vorschlägen beim Tippen. Das geht im
  Wertedialog eines Skizzenmaßes, in „Linienlänge“ und „Kreis-/Bogenradius“ der gewählten
  Skizzengeometrie und in „Distanz“ und „Versatz“ der Extrusion.
  - Einen unbekannten Namen legt Völund nach kurzer Frage mit dem bisherigen Wert an.
  - Gebundene Maße zeigen `A = 20,000 mm`.
  - Eine Zahl im Feld löst die Bindung.
- **Ändern:** Ändert man einen Wert in der Tabelle, rechnet das ganze Modell neu, als ein
  Rückgängig-Schritt.
- **Löschen:** Einen benutzten Parameter löscht Völund nur nach Rückfrage. Die Maße behalten
  dann ihren aktuellen Wert als feste Zahl.

## Messen

- „Messen“ (Taste I) mit zwei Klicks: Länge, Radius, Fläche, Volumen eines Elements; Abstand und
  Winkel zwischen zwei Elementen. Drei Nachkommastellen, Kopieren in die Zwischenablage mit
  voller Genauigkeit. Messen verändert das Dokument nicht.
- **Grenzen:** Keine Eckpunkte, keine Toleranzprüfung, keine Zeichnungsbemaßung.

## Austausch (STEP)

- **Importieren (STEP)…** (Datei-Menü, Strg+I) fügt die Volumenkörper einer STEP-Datei als
  einen Schritt „Import N“ ins offene Dokument ein, ein Rückgängig-Schritt. Jeder Volumenkörper
  wird ein eigener Körper mit dem Namen aus der Datei. Dateien in Zoll, Meter usw. werden in mm
  umgerechnet; die Statuszeile nennt die Einheit. Die Körper sind danach gewöhnliche Körper:
  verrunden, bohren, bewegen usw.
- **Öffnen** nimmt auch `.step`/`.stp` und legt dafür ein neues Dokument mit dem Import an.
- Die Volumenkörper werden im Dokument gespeichert. Die STEP-Datei wird danach nicht mehr
  gebraucht.
- Flächen, Schalen oder Kurven ohne Volumen und ungültige Volumenkörper werden nicht übernommen;
  ein Hinweis nennt ihre Zahl. Eine unlesbare Datei meldet einen Fehler und ändert nichts.
- **Exportieren…** (Strg+E) schreibt den im Baum gewählten Körper, sonst alle sichtbaren Körper,
  in mm mit ihren Namen. Das Format folgt dem gewählten Dateityp.
  - STEP und IGES schreiben die exakten Körper.
  - DXF schreibt die gewählte Skizze als Zeichnung in ihren eigenen Ebenenkoordinaten.
    Hilfsgeometrie liegt auf dem Layer „Konstruktion“. Ohne gewählte Skizze bittet Völund, erst
    eine zu wählen.
  - STL und 3MF schreiben Dreiecksnetze für den 3D-Druck. Vorher fragt Völund die Auflösung ab:
    grob (0,1 mm), mittel (0,025 mm) oder fein (0,005 mm) als größte Abweichung von der Fläche.
    Die letzte Wahl wird gemerkt.
  - Mehrere Körper kommen in eine Datei und bleiben getrennt. STL enthält alle Dreiecke, 3MF ein
    Objekt je Körper mit seinem Namen.
  - Hat das Modell Fehler, wird der Export abgelehnt – auch wenn noch eine ältere gültige Form
    angezeigt wird.
- **Grenzen:** Aus STEP entsteht keine parametrische Historie. Baugruppenstruktur, Farben,
  Ebenen und PMI werden nicht übernommen. Kanten importierter Körper lassen sich nicht in Skizzen
  projizieren oder als Achse nutzen (Verrunden und Fasen gehen). Ein Import darf höchstens 48 MiB
  Geometrie enthalten.

## Weitere Grenzen von v1

- Parameter binden bisher Skizzenmaße und Extrusionen (Distanz, Versatz), keine Bohrungen,
  Verrundungen, Fasen, Muster oder Grundkörper; keine Ausdrücke wie `A * 2`.
- Keine Baugruppen, keine Zeichnungen, keine Blechfunktionen, keine Freiformflächen.
- Mehrfachauswahl gibt es für Kanten und Skizzenelemente, nicht für ganze Körper beim Bewegen.
- Technische Kernelmeldungen sind teils englisch; die häufigen Fälle sind deutsch erklärt.
