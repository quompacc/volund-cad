# 13 – Arbeitsabläufe nach SolidCraft und Fusion (Reihe W)

**Stand:** 23.10.2026.

**Auftrag des Nutzers (23.10.2026):**

- Die Bedienung der Werkzeuge folgt Fusion und Shapr3D. Vorlage im Code ist SolidCraft, ein
  quelloffener Fusion-Nachbau (MIT/Apache).
- Den ganzen Arbeitsablauf übernehmen, wo er uns dient.
- Unsere Oberfläche bleibt: Karten, Werkzeugleiste, Zeitleiste und Aufbau. Das Innenleben bleibt
  ebenfalls, wo es besser ist.
- Der Agent entscheidet selbst und schreibt die Entscheidungen in den Nachweis.

Die Reihe W kommt vor N07–N15. Die Vorlage ist in [`../workflow-reference.md`](../workflow-reference.md)
beschrieben. Lücken, die die Bestandsaufnahme vom 23.10.2026 fand:

- kein Extrusionspfeil;
- keine getippten Werte beim Bewegen;
- fast keine Einzeltasten;
- keine 3D-Boxauswahl;
- kein Kontextmenü im Baum;
- Bemaßung nur über Auswahl und Befehl;
- Werte nur im Panel.

| Paket | Inhalt | Stand |
|---|---|---|
| W01 (= D01) | Werte beim Skizzieren an der Maus: Tab, Enter, getippte Werte werden Maße | gebaut (`f30fc41`) |
| W02 | Bemaßung wie Fusion. Werkzeug **D**: Element klicken, Wert sofort tippen, Enter. Zweites Element macht daraus Abstand oder Winkel. Doppelklick auf ein Maß ändert es im Feld an der Maus statt im Dialog. | gebaut (`b18dc2d`) |
| W03 | Extrudieren mit Pfeil und Wert an der Pfeilspitze. Ein einzelnes Profil wird selbst gewählt. Vereinigen oder Abziehen ergibt sich aus der Zugrichtung. **Q** öffnet Drücken/Ziehen, bei Kanten Verrunden. | gebaut (`2e0d590`) |
| W04 | Einzeltasten wie Fusion. E Extrudieren, Q, F Verrunden, H Bohrung, M Bewegen, I Messen, D Bemaßung, X Hilfsgeometrie, Shift+R wiederholen, V Sichtbarkeit, F2, F6 Einpassen, Entf global. Skizzenwerkzeug-Tasten starten außerhalb einer Skizze eine Skizze. | teilweise (`2e0d590`) |
| W05 | Werte an jedem Griff tippen (Verrunden, Fase, Schale, Bezugsebene, Drücken/Ziehen). Nach einer Auswahl nimmt das Wertfeld sofort die Tastatur. | teilweise (`4fb575c`) |
| W06 | Bewegen/Kopieren als Werkzeug mit X/Y/Z und Winkeln zum Tippen, Griffen, Rasterung und „Kopie erzeugen“. | teilweise (`c79917d`) |
| W07 | Auswahl: Kante vor Fläche, Doppelklick wählt die Tangentialkette, gleicher Ort erneut geklickt wählt den nächsten Kandidaten, 3D-Boxauswahl (Körper, im Werkzeug dessen Eingabe), Auswahlfilter. | geplant |
| W08 | Zeitleiste: Doppelklick bearbeitet mit vorübergehendem Zurückrollen. Abbrechen stellt die Marke wieder her. Kontextmenü mit Bearbeiten, Umbenennen, Fehler kopieren. | geplant |
| W09 | Kontextmenüs im Modellbaum für Körper, Skizze und Ebene, wie bei SolidCraft. | gebaut (`2d6db2d`) |
| W10 | Skizze: zweimal Esc beendet sie, eine leere Skizze wird verworfen. Rechtsklick beendet das Werkzeug. X schaltet Hilfsgeometrie auch mitten im Werkzeug um. Fang mit Mitte, parallel, senkrecht, Verlängerung und Tangente samt Hilfslinien. | teilweise |
| W11 | Direkteingabe für Bogen, Langloch, Vieleck und Ellipse. Felder neben dem Element. Ein getippter Winkel wird Winkelmaß. | gebaut (`77e4e76`) |
| W12 | Normbemaßung wie in FreeCAD und Fusion: Maßhilfslinien, Maßlinie mit Pfeilen neben der Geometrie, kompakter Wert darauf. Die Felder der Direkteingabe sitzen beim Zeichnen auf solchen Maßlinien. | in Arbeit |
| W13 | Rechteck mit den Optionen „Abgerundete Ecken“ (U) und „Rahmen“ (J) wie in FreeCAD. Alle Beziehungen und Maße werden gesetzt, das Rechteck ist sofort voll bestimmt. Eingabe in drei Schritten (Ecken, Radius, Rahmen nach innen oder außen). | in Arbeit |
| W14 | Ebenen klein und weg mit der ersten Skizze; Skizzen bleiben sichtbar, bis ein Schritt sie verwendet, und haben ein Auge im Modellbaum (wie in Fusion). | in Arbeit |

**Arbeitsregeln:**

- Wie bisher je Paket: Code, Tests (Oberflächentests als echte Abläufe auf Fedora) und Nachweis
  `evidence/W??.md`. Abgenommen wird nach grünem CI-Lauf.
- Keine Rückfragen beim Nutzer. Abweichungen von der Vorlage stehen mit Grund im Nachweis.
