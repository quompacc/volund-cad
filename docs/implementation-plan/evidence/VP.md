# VP01–VP05 – Befunde der Nutzerprüfung

**Status:** umgesetzt, Abnahme durch den Nutzer steht aus. Plan und Entscheidungen:
[Kapitel 15](../15-v1-pruefung.md).

## Prüfung

`tests/desktop/SketchBasicsTests.cpp` (`desktop.sketch_basics`) spielt die Abläufe des Nutzers mit
echten Mausereignissen durch:

1. Rechteck zeichnen; die Ecke unter dem Zeiger leuchtet auf.
2. Klick mitten auf die Linie wählt die Linie, Klick auf die Ecke den Punkt.
3. Maß zwischen zwei Ecken einer Kante: `pointDistance` 60 mm.
4. Maß zwischen zwei diagonalen Ecken: Das Menü fragt; „senkrecht“ ergibt `pointDistanceY` 30 mm.
5. Das Fenster nennt die fehlende Lage.
6. Ziehen an der Linie um (25, 20), dann an der Ecke um (−20, −10): Die Maße bleiben, die linke Seite
   steht danach bei −25 mm (im Dokument geprüft).
7. Extrudieren, auf der Oberseite skizzieren, zwei Körperkanten projizieren, Maß von einer Ecke des
   neuen Rechtecks zu einer Ecke der Körperkante, 20 mm eintippen.

Bei gesetztem `VOELUND_SKETCH_SHOTS` entstehen Bilder jedes Schritts, im dunklen Design. Sie wurden
bei `QT_SCALE_FACTOR=1.5` auf einem virtuellen Linux-Bildschirm angesehen, passend zum Bildschirm des
Nutzers.

## Was erst die Bilder gezeigt haben

Kein Test hätte diese Fehler gemeldet; alle sind behoben:

- Bei gewählter Linie listete das Fenster Drehwinkel, Skalierfaktor, Anzahl im Muster und mehr.
- Das Ziehen an der Ecke bewegte nichts, obwohl der Ablauf durchlief.
- Der Punkt blieb nach dem Ziehen an der alten Stelle sichtbar, OK blieb gesperrt.
- Text wurde rechts abgeschnitten, sobald das Fenster einen Rollbalken bekam; eine leere Überschrift
  „Einstellungen“ blieb stehen.
- Die weiße Körperfläche ließ sich nachstellen. Ein mattes Material machte sie fast schwarz; übernommen
  wurde deshalb das bisherige Material ohne Glanz.

## Grenzen

Nicht geprüft: das echte Fenster unter Windows (nur gebaut und installiert, keine GUI-Tests auf dem
Windows-Rechner). Das Menü aus VP-E3 öffnet sich im virtuellen Wayland-Testaufbau nicht sichtbar
(`Failed to create grabbing popup`); der Test löst seine Einträge direkt aus. Die übrige Oberfläche
wurde nach der Schriftänderung nur stichprobenartig angesehen. Offene Punkte: Kapitel 15.
