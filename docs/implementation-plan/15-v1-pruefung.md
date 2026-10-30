# 15 – Befunde aus der Nutzerprüfung der v1 (Serie VP)

**Stand: 30. Oktober 2026.** Der Nutzer hat den Kandidaten 1.0.0-rc.1 auf einem 4K-Bildschirm mit
150 % Skalierung geprüft und die Freigabe **abgelehnt**. Diese Befunde gehen vor den Serien I und B
aus [Kapitel 14](14-step-und-baugruppen.md). Die Freigabe wird erst wieder erfragt, wenn der Nutzer
die Grundlagen selbst ausprobiert hat.

## Befunde des Nutzers

1. Die Schrift ist kaum lesbar: zu dünn und zu klein.
2. Das Skizzenfenster ist unverständlich: Listen „Beziehungen“ und „Elemente“, „Linientyp“, ein
   schwarzer Balken. Nicht voll bestimmte Elemente lassen sich nicht verschieben. Es ist nicht
   erkennbar, was noch fehlt.
3. Die Symbole der Skizzenleiste sind viel zu klein.
4. Ecken und Punkte lassen sich nicht auswählen, um zwischen ihnen ein Maß zu setzen.

## Warum das durch die Tests gekommen ist

Die Tests prüften, ob eine Funktion existiert, und setzten Zustände oft direkt. Niemand hat das
Ergebnis in der Skalierung des Nutzers angesehen. Der Ablauf „zeichnen, bemaßen, ziehen“ war nie als
Ganzes getestet, und ein Rechteck mit Maßen ließ sich tatsächlich nicht ziehen.

**Neue Arbeitsregel:** Jede Änderung an der Oberfläche wird als echtes Bild bei 150 % im dunklen
Design angesehen, und Abläufe werden so getestet, wie ein Mensch sie ausführt
(`tests/desktop/SketchBasicsTests.cpp`, Bilder mit `VOELUND_SKETCH_SHOTS=<Ordner>`).

## Pakete

| Paket | Inhalt | Ursache |
|---|---|---|
| VP01 | Ziehen unter Bedingungen | Der Punkt wurde vor dem Lösen bewegt: Linien mit Bindung „waagerecht“ waren damit ungültig und der Zug wurde still verworfen. Ohne Haltepunkt zog der Löser ihn zudem zurück. |
| VP02 | Punkte und Linien auswählen, Maß zwischen Punkten | Jeder Klick nahm den nächsten Endpunkt, Linie und Ecke waren nicht unterscheidbar; das Maß-Werkzeug prüfte Winkel zuerst. Die Mausposition ist zudem aufs Raster gerundet und taugt nicht für den Nähevergleich. |
| VP03 | Skizzenfenster | Entwicklersicht statt Bedienung: alle Werte aller Bearbeitungen sichtbar, Balken ohne Beschriftung. |
| VP04 | Schrift und Symbole | Feste Größen 11–13 px, 20-px-Symbole, schwacher Kontrast. |
| VP05 | Überstrahlte Körperfläche | Der Glanzanteil des Materials macht eine zur Kamera zeigende Fläche weiß. |

## Entscheidungen

- **VP-E1 Klick auf Linie gegen Ecke:** Innerhalb von 12 Bildschirmpixeln um eine Ecke oder einen
  Mittelpunkt gilt der Punkt, sonst das Element. Der Punkt leuchtet vor dem Klick auf. Der Vergleich
  läuft in Pixeln, nicht in Skizzeneinheiten.
- **VP-E2 Bezüge bleiben:** Auch ein als Ganzes gewähltes Element behält intern seinen nächsten Punkt
  als Bezug. So funktionieren „Fixieren“ und die anderen Beziehungen weiter; gezogen wird es aber als
  Ganzes.
- **VP-E3 Maß zwischen zwei Punkten:** Liegen sie in beiden Richtungen auseinander, fragt ein kleines
  Menü: waagerecht, senkrecht oder direkt, jeweils mit dem gemessenen Wert. Fusion wählt das über die
  Mausposition. Das Menü ist die einfachere Lösung und kann später ersetzt werden.
- **VP-E4 Skizzenfenster:** Sichtbar bleiben Zustand, was fehlt, „Hilfslinie“ und die Werte der
  Auswahl. Listen, Ansichtsschalter und Werte für Drehen, Skalieren, Versatz und Muster liegen unter
  „Details und Ansicht“. Eine Bearbeitung mit Werten öffnet diesen Bereich selbst.
- **VP-E5 Größen:** Alle Schriftgrößen +2 px (Grundschrift 15 px), Stärke 500, höherer Kontrast.
  Skizzensymbole 30 px auf 46-px-Knöpfen, Werkzeugleiste 22 px, Karte rechts 390 statt 344 px.
  Unter dem Grenzwert bleibt die Leiste am »-Menü (L08); sie wird nie abgeschnitten.

## Noch offen

- Ein Maß zum **Ursprung** und zu **nicht projizierten Körperkanten** ist nicht direkt möglich. Die
  Körperkontur beim Skizzieren auf einer Fläche ist nur Anzeige; ihre Kanten müssen erst mit
  „Projizieren“ übernommen werden. Die Lage legt man heute mit „Punkt fixieren“ (Schloss) fest.
  Fusion macht beides direkt; das ist der nächste Schritt für diese Serie.
- Die Maßart per Mausposition (VP-E3).
- Andere Fenster wurden durch die größere Schrift nur am Rande mitgeprüft. Der Nutzer entscheidet
  bei seiner nächsten Prüfung, was noch nicht passt.
- Windows: Schrift und Bedienung wurden auf Linux bei 150 % angesehen, unter Windows nur gebaut. Die
  Wirkung der Schriftstärke 500 unter Windows bewertet der Nutzer.
