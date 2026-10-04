# UI-Konzept (UX01)

Stand 04.10.2026. Teil 1 ist die Bestandsaufnahme. Teil 2, das Zielbild, wird mit dem
Nutzer erarbeitet und erst nach seiner ausdrücklichen Freigabe als ADR festgehalten.
Aufnahmen liegen in `artifacts/ux01-inventory/` (nicht versioniert). Erzeugt werden sie
mit `voelund_ui_inventory_capture` und dem Smoke-Test der Vorschau.

## Teil 1 – Bestandsaufnahme

### Zwei Oberflächen nebeneinander

| | Produkt `voelund-cad` | Vorschau `voelund-ui-preview` |
|---|---|---|
| Rahmen | QMainWindow, nativer Stil (hell auf Windows, Breeze auf KDE) | Eigenes dunkles Grün/Gold-Theme (QSS), feste Höhenaufteilung |
| Befehle | Nur Menü „File“; Toolbars History, SOLID (jeder Befehl doppelt als Knopf und im Menü), Connection, Transform/Snap | Kopfzeile, Arbeitsbereichs-Tabs, Ribbon mit Gruppen und Dropdowns |
| Modell | Dock „Model“: Wurzel „Body“, flache Featureliste ohne Symbole, ohne Kontextmenü | „Browser“ mit Filter, Symbolen und „Origin“-Eintrag (ohne Funktion) |
| Werkzeuge | Docks „Properties“ und „Tool“ übereinander als Reiter | Schwebender Inspektor im Viewport (z. B. Extrude) |
| Navigation | Rechts drehen, Mitte verschieben, Rad zoomen; kein ViewCube, keine Standardansichten | ViewCube mit Top/Front/Right/ISO/FIT/Ortho |
| Historie | – | Timeline-Leiste mit festem Platzhaltertext |
| Funktion | Real: Box, Zylinder, Skizze, Extrusion, Speichern, Undo, Reparatur | Nur Skizze echt; Extrude-OK, Revolve, Hole, Fillet, Chamfer, Shell, Joint sind Attrappen |

Im Plan galt bisher die Gestaltung der Vorschau als „genehmigt“ (dunkle Palette nach
Völund Vault, Fusion-ähnlicher Rahmen). Sie wurde aber nie ins Produkt übernommen. Seit
dem 04.10.2026 gilt sie nicht mehr als Grundlage; das Zielbild entsteht neu in UX01.

### Brüche im Arbeitsablauf (Produkt)

1. **Kein klarer Einstieg.** Die Anwendung startet mit Box und Zylinder. Ein leeres
   Dokument gibt es nur über File → New, und es gibt keine Startansicht.
2. **Doppelte Wege.** „Sketch“ und „Sketch Rectangle“ (alter Push/Pull-Weg) stehen
   nebeneinander. Jeder Befehl erscheint zweimal. Die Vorschau hat einen zweiten
   Skizzen-Erzeugungsweg (Befund F11).
3. **Auswahl ist schwer verständlich.** Basisebenen liegen neben Körpern und fangen Klicks
   ab (Aufnahme 02: Klick auf die Box traf die XZ-Ebene; der Editor startete auf der
   falschen Ebene). Profile wählt man durch Klick auf eine Skizzenkante, die nur sichtbar
   ist, wenn die Skizze im Baum gewählt ist (Aufnahmen 04/05).
4. **Modellbaum ohne Bedienung.** Keine Symbole, kein Umbenennen, Löschen, Ausblenden oder
   Unterdrücken, kein Kontextmenü. Fehlerzustände erscheinen nur als Text-Suffix
   „[error]“/„[blocked]“.
5. **Werkzeuge und Eigenschaften konkurrieren.** Das Werkzeugpanel liegt als zweiter
   Reiter hinter „Properties“. Der Skizzeneditor bietet rund 45 Knöpfe in fünf Reitern
   gleichzeitig an.
6. **Globale statt objektbezogener Einstellungen.** „Connection“ ändert immer den
   letzten Zylinder, nicht den gewählten, und jede Änderung ist ein Undo-Schritt.
   Snap-Werte stehen dauerhaft in der Toolbar.
7. **Navigation.** Es gibt keinen ViewCube, keine Standardansichten, kein „Alles
   zeigen“. Die Kamera kehrt nach einer Skizze nicht zurück.
8. **Tastatur.** Es gibt keine Entf-Taste, kein Escape außerhalb von Werkzeugen und
   keine Ansichtstasten. Strg+Z/Y sind teils doppelt belegt. Undo/Redo sind gesperrt,
   wenn nichts gewählt ist.
9. **Darstellung.** Gemischt: heller nativer Stil mit dunklen Fehlerfarben, grün-dunkler
   Viewport, kein einheitliches Farb- und Symbolsystem. Die Texte sind nur teilweise
   übersetzbar; Deutsch gibt es nicht.
10. **Fehler und Reparatur.** Die Reparatur ist nur über Toolbar-Knöpfe und Dialoge
    erreichbar. Der Status „Last valid geometry …“ in der Statusleiste ist schwer zu
    verstehen.

### Bereits festgelegte Nutzervorgaben

- **Basisebenen:** An das Objekt gebunden (Ursprung je Körper bzw. Komponente) und im
  Modellbaum ein- und ausblendbar. Im leeren Dokument sind sie sichtbar, neben Körpern
  nur dezent (Rückmeldung zu V01b vom 04.10.2026).
- **Parametertabelle:** Mit einem Klick erreichbar, Eingabe `=A` in Maßfeldern
  ([Konzept](parameter-table.md)).
- **Mehrere Körper** direkt im Dokument, ohne vorher eine Komponente anzulegen
  (A-BODY-03).
- **Abstimmung:** Sichtbare Änderungen werden vorher mit dem Nutzer abgestimmt;
  Aufnahmen werden zur Durchsicht vorgelegt.

### Fragen für das Zielbild

1. **Vorbild:** Woran soll sich die Bedienung orientieren? Fusion 360 (Ribbon,
   Browser, Timeline, schwebende Panels), Onshape, SolidWorks oder FreeCAD (Workbenches)?
   Was gefällt dir dort, was stört?
2. **Erscheinung:** Dunkles Theme wie in der Vorschau (Grün/Gold), neutral dunkel oder
   hell? Gilt die Vorschau-Optik überhaupt noch als Richtung?
3. **Befehle:** Ribbon mit Gruppen oder schlanke Toolbar mit Kontextpanels? Arbeitsbereiche
   (Solid, Skizze, später Baugruppe) als Tabs?
4. **Historie:** Timeline unten wie in Fusion oder Feature-Baum wie in FreeCAD und
   SolidWorks, oder beides?
5. **Panels:** Werkzeuge als schwebender Inspektor am Viewport oder fest angedockt rechts?
6. **Sprache:** Deutsch als Oberflächensprache, Englisch oder umschaltbar?
7. **Skizzenmodus:** Eigene Werkzeugleiste beim Skizzieren; Constraints als Symbole am
   Element oder als Panel?

## Teil 2 – Zielbild

Offen; wird mit dem Nutzer erarbeitet (klickbare Mockups), danach ADR und Anpassung von
U02–U12 und S13.
