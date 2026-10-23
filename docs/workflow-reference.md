# Arbeitsablauf-Vorlage: SolidCraft und Fusion

Stand: 23.10.2026. Grundlage ist eine Lesestudie des quelloffenen SolidCraft
(<https://github.com/tangency-labs/solidcraft>, MIT oder Apache-2.0, ein Fusion-Nachbau in Rust).
Kein Code ist übernommen, nur das Verhalten. Die Reihe W
([Kapitel 13](implementation-plan/13-arbeitsablaeufe.md)) setzt diese Vorlage in Völund um.

## Skizze

- **Wertfelder beim Zeichnen.** Sie erscheinen ab dem ersten Punkt.
  - Linie: Länge und Winkel. Rechteck: Breite und Höhe. Kreis: Durchmesser.
  - Bogen: Radius, dann Öffnungswinkel. Vieleck: Radius und Seiten. Langloch: Länge, dann Breite.
  - Das erste Feld hat sofort die Tastatur, der Live-Wert ist markiert. Tippen ersetzt ihn.
  - Tab und Umschalt+Tab wechseln die Felder, Enter setzt.
  - Getippte Werte sind fest, freie folgen der Maus. Bei Rechtecken bestimmt die Maus die Seite.
  - Werte dürfen Ausdrücke sein. Getippte Werte werden treibende Maße und entstehen im selben
    Rückgängig-Schritt wie das Element.
- **Linienkette.** Jeder Klick setzt ein Segment. Esc oder Rechtsklick beendet die Kette, ein
  zweites Esc das Werkzeug. Zweimal Esc schnell hintereinander beendet die Skizze; eine leere
  Skizze wird verworfen.
- **Fang**, in dieser Reihenfolge:
  1. Körperecke.
  2. Linienmitte.
  3. Tangente.
  4. Auf Kurve.
  5. Horizontal oder vertikal (±2,5°).
  6. Parallel, senkrecht oder Verlängerung, mit gestrichelter Hilfslinie.
  7. Raster.

  Strg oder Alt schaltet den Fang ab. Beim Setzen wird die passende Beziehung angelegt; eine
  überzählige fällt weg.
- **Bemaßung (D).** Element klicken, der gemessene Wert steht markiert im Feld. Tippen und Enter.
  - Ein zweites Element vor dem Tippen macht daraus Abstand oder Winkel.
  - Die Lage des Texts wählt horizontal, vertikal oder ausgerichtet.
  - Ein Doppelklick auf ein Maß ändert es direkt am Ort.
  - Ein Maß, das überbestimmen würde, wird Referenzmaß.
- **Beim Betreten** dreht die Ansicht auf die Skizzenebene. „Skizze beenden“ lässt die Ansicht,
  wie sie ist.

## Modell

- **Extrudieren (E) und Drücken/Ziehen (Q).**
  - Pfeil am Profilschwerpunkt, das Wertfeld an der Spitze nimmt sofort die Tastatur.
  - Ein einzelnes Profil wird selbst gewählt.
  - Vereinigen oder Abziehen ergibt sich aus der Zugseite; eine eigene Wahl beendet das.
  - Q wählt Verrunden bei Kanten, sonst Extrudieren.
- **Werkzeugkarten.** Vorgewähltes füllt die Eingaben. Nach einer Auswahl geht der Fokus zur
  nächsten Eingabe. Der Hauptwert hat einen Pfeil mit Feld im Ansichtsfenster. Enter übernimmt,
  Esc bricht ab. Die Vorschau zeigt Hinzugefügtes eingefärbt und Entferntes als Geist.
- **Verrunden und Fase (F).** Kantenketten werden tangential mitgenommen. Ein Pfeil sitzt auf der
  Winkelhalbierenden; es gibt variable Radien.
- **Bohrung (H).** Platzierung auf einer Fläche, wobei ein Klick an einen Skizzenpunkt fängt. Dazu
  Typ, Tiefe und Gewinde.
- **Bewegen/Kopieren (M).** Arten: frei, verschieben, drehen, Punkt zu Punkt. X/Y/Z und Winkel lassen
  sich tippen, die Griffe rasten ein; dazu „Kopie erzeugen“.
- **Muster.** Richtungen mit Zugpfeilen für den Abstand, Körper oder Features als Vorlage.
- **Bezugsgeometrie.** Ebene mit Abstand und Pfeil, unter Winkel, Mittelebene, durch drei Punkte;
  dazu Achsen und Punkte.

## Auswahl, Zeitleiste, Baum

- **Auswahl.**
  - Kante vor Fläche.
  - Ein Klick ersetzt die Auswahl, Strg oder Umschalt schaltet hinzu oder weg.
  - Ein erneuter Klick an derselben Stelle wählt den nächsten Kandidaten.
  - Boxauswahl links→rechts ist ein Fenster, rechts→links kreuzend; außerhalb einer Skizze wählt
    sie Körper.
  - Doppelklick auf eine Kante wählt die Tangentialkette.
  - Es gibt einen Auswahlfilter.
- **Zeitleiste.**
  - Ein Doppelklick rollt vorübergehend zurück und öffnet das Feature; Abbrechen stellt die Marke
    wieder her.
  - Kontextmenü: Bearbeiten, Umbenennen, Unterdrücken, Marke hierher, Löschen, Fehler kopieren.
- **Baum.** Kontextmenüs für Körper (Bewegen, Darstellung, Export, Löschen …), Skizze (Extrudieren,
  Bearbeiten, Ebene neu wählen, DXF …) und Ebene (Skizze anlegen …).
- **Tasten.**
  - Skizze: L, R, C, D, X, O, T, P.
  - Modell: E, Q, F, H, M, I.
  - Allgemein: S Suche, V Sichtbarkeit, F2, F6, Shift+R wiederholen.
- **Rückgängig.** Jede übernommene Aktion ist ein Schritt. Vorschauen und vorübergehendes
  Zurückrollen erscheinen nicht in der Historie.
