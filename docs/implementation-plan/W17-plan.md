# W17–W21 – Skizze auf SolidCraft-/Fusion-Niveau

Auslöser: Nutzer-Rückmeldung vom 25.10.2026 mit FreeCAD-Vergleichsbild: In der Skizze ist viel
weniger Information zu sehen als bei FreeCAD, die Schrift ist extrem klein, die Werkzeugauswahl
der Skizze ist „noch lange nicht fertig“. Vorlage ist SolidCraft (näher an Fusion als FreeCAD);
der Nutzer testet nicht selbst, der Agent prüft alles eigenständig (Bildschirmfotos auf Fedora
mit `voelund_sketch_capture`, auch mit `QT_SCALE_FACTOR=2`).

Referenz: Auswertung des SolidCraft-Quellcodes (nur gelesen, `_ref/solidcraft`), Stand
25.10.2026. Die Werte unten sind SolidCraft-Werte in logischen Punkten.

## W17 – Lesbar auf jedem Bildschirm

- OCCT bekommt die Bildschirmskalierung (`RenderingParams.Resolution = 72 × Skalierung`):
  Schrift, Punktmarken und Linienbreiten in Bildschirm-Pixeln wachsen auf 200 % mit.
- Eigene Pixelmaße der Skizzenbeschriftung (Versatz, Stapelung, Maßpfeile, Trefferfläche)
  folgen derselben Skalierung.
- Größen wie SolidCraft: Maßzahlen 12–15 pt, Pfeile 9–11 px, Beziehungssymbole 15 px.

## W18 – Beziehungen sichtbar an der Geometrie

- Jede Beziehung (außer Maßen) als kleines Symbol-Kästchen (15 px, Rand, Hintergrund) an der
  Geometrie: Punkt-Beziehungen am Punkt, Linien-Beziehungen in der Mitte der Linie,
  Zweier-Beziehungen (parallel, senkrecht, gleich, tangential, konzentrisch, kollinear) mit je
  einem Symbol an beiden Elementen. H/V als Buchstaben.
- Platz: seitlich der Linie (0,9 × Symbolgröße), bei Überdeckung entlang der Linie weiter, bei
  großem Abstand eine dünne Führungslinie.
- Maus über einem Symbol hebt die beteiligten Elemente hervor, Tooltip mit Namen; Klick wählt,
  Entf löscht. Rot bei Konflikt, blass bei überzählig.
- Farben der Geometrie: frei blau, bestimmt dunkel/hell, Hilfsgeometrie orange gestrichelt,
  projiziert violett, fixiert grün.

## W19 – Skizzen-Karte wie die SolidCraft-Palette

- Kopf: Skizzenname, Status („Voll bestimmt“, „N Freiheitsgrade“, „Beziehungen im Konflikt“) mit
  Fortschrittsbalken (bestimmter Anteil).
- Warnungen „N offene Enden“ / „N nur lageweise verbunden“ mit Knopf „Verbinden“.
- Varianten des laufenden Werkzeugs als Symbolknöpfe (Rechteck: 2 Punkte, 3 Punkte, Mitte,
  abgerundet; Kreis: Mitte, 2/3 Punkte, tangential ...).
- Linientyp: Normal, Hilfsgeometrie, Mittellinie, unendlich (wandelt Auswahl um oder setzt den
  Zeichenmodus).
- Schalter: Beziehungen, Maße, Punkte, Fang, Hilfsgeometrie-Modus (X); „Mehr“ mit Raster,
  Profil, projizierte Geometrie.
- Fuß: großer Knopf „Skizze beenden“, „Draufsicht“.
- Statuszeile in der Ansicht oben links: „Skizze 1 — voll bestimmt / N Freiheitsgrade“.
- (Über SolidCraft hinaus, wie FreeCAD:) aufklappbare Listen der Beziehungen und Elemente mit
  Auswahl, Umbenennen, treibend/getrieben, Löschen.

## W20 – Werkzeugauswahl der Skizze vollständig

- Gruppen wie SolidCraft: ERSTELLEN (Linie, Rechteck-Familie, Kreis-Familie, Bogen-Familie,
  Langloch, Vieleck, Ellipse, Spline, Punkt, Text), ÄNDERN (Stutzen, Versatz, Abrunden, Fase,
  Verlängern, Trennen, Skalieren, Verschieben/Kopieren), MUSTER (Spiegeln, kreisförmig,
  rechteckig), BEZIEHUNGEN (Bemaßung D, H/V, Koinzident, Tangential, Gleich, Parallel,
  Senkrecht, Fixieren, Mittelpunkt, Konzentrisch, Kollinear, Symmetrie), REFERENZ (Projizieren
  P, Schnitt), BEENDEN.
- Familien-Knopf: oben startet die zuletzt benutzte Variante, unten Pfeil mit Liste.
- Tasten: L, R, C, D, X, O, T, P, Shift+R; Esc bricht ab, zweimal Esc beendet die Skizze.
- Hinweis in der Statuszeile je Werkzeug („Linie: Punkte klicken; Rechtsklick oder Esc
  beendet“ ...).
- Rechtsklick-Menü in der Skizze: Wiederholen, Werkzeuge, nur die Beziehungen, die zur Auswahl
  passen (1 Linie: H/V; 2 Linien: parallel, senkrecht, kollinear, gleich ...).

## W21 – Maße wie SolidCraft

- Maßzahl in Monospace mit leicht deckendem Kästchen, Pfeile innen, bei Platzmangel außen mit
  Führung; Versatz 22 px von der Geometrie, Hilfslinien 3 px Abstand, 4 px Überstand.
- Getriebene Maße in Klammern und grau; ein überbestimmendes Maß wird automatisch getrieben.
- Ausdruck-Maße mit „fx: “ davor.
- Maßzahl ziehen verschiebt die Maßlinie (ein Rückgängig-Schritt).

## Prüfung

Jedes Paket mit Oberflächentest als echter Ablauf auf Fedora und Bildschirmfoto bei 100 % und
200 % (`voelund_sketch_capture`), Nachweis `evidence/W??.md`. Abnahme nach grüner CI.
