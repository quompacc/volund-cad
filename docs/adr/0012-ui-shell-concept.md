# ADR 0012: Schlanke Arbeitsoberfläche mit Werkzeugleiste, Kontextleiste und Timeline

**Status:** accepted – ausdrückliche Nutzerfreigabe des klickbaren Prototyps
**Datum:** 4. Oktober 2026
**Owner:** Völund-Projekt
**Ersetzt:** die „genehmigte Gestaltung“ der UI-Vorschau als Grundlage von U03
**Ergänzt:** [ADR 0002](0002-modular-workspaces-and-command-registry.md), [ADR 0003](0003-generic-tool-panel-lifecycle.md)

## Kontext

Das Produkt `voelund-cad` und die UI-Vorschau `voelund-ui-preview` haben zwei
verschiedene Oberflächen. Keine von beiden ergibt einen verständlichen Arbeitsablauf
(Bestandsaufnahme in [ui-concept.md](../ui-concept.md), Teil 1). Mit dem Nutzer wurde
ein klickbarer Prototyp erarbeitet (Ebene → Skizze → Bereich → Extrusion, Parameter,
Timeline, helle und dunkle Darstellung). Der Nutzer hat ihn am 04.10.2026 freigegeben.
Seine Bedingung: Die Oberfläche wird einfach, das Werkzeug bleibt ein vollparametrisches
CAD.

## Entscheidung

Eine einzige produktive Oberfläche in `voelund-cad` mit diesem Aufbau:

- **Kopfzeile:** Dokumentname und Speicherzustand, Rückgängig/Wiederholen, Befehlssuche
  (Strg+K) über alle registrierten Befehle, Schalter für Werkzeugleiste und Parameter,
  Umschalter hell/dunkel.
- **Modellbaum links:** Ursprung des Dokuments mit XY/XZ/YZ und dem Raster. Darunter
  die Körper, jeder mit eigenem Ursprung, eigenen Skizzen und Features. Jeder Eintrag,
  der sich anzeigen lässt, hat einen Augen-Schalter. Sichtbarkeit ist Ansichtszustand:
  Sie erzeugt keinen Undo-Schritt und macht das Dokument nicht „geändert“.
- **Viewport** über die volle Fläche, mit ViewCube. Das Raster auf XY ist im leeren
  Dokument eingeschaltet und lässt sich im Baum und im Viewport ausschalten.
- **Werkzeugleiste:** senkrecht am linken Viewportrand, ausblendbar. Sie zeigt alle
  Volumenwerkzeuge. Was zur Auswahl nicht passt, ist gesperrt, nicht versteckt.
- **Werkzeugleiste je Modus** (Nutzerentscheidung vom 04.10.2026, nach U05): Die
  Werkzeugleiste zeigt die Werkzeuge der gerade genutzten Funktion. Im Skizzenmodus sind
  das Zeichenelemente, Beziehungen, Bemaßungen und Bearbeitungswerkzeuge, außerhalb davon
  die Volumenwerkzeuge. Weitere Modi folgen demselben Muster.
- **Kontextleiste:** schwebt unten. Sie zeigt die Schritte, die zur aktuellen Auswahl
  passen; der nächste sinnvolle Schritt ist hervorgehoben. Im Skizzenmodus trägt sie
  die Skizzenwerkzeuge und „Skizze fertig“.
- **Werkzeugpanel:** schwebt am Viewport, mit OK und Abbrechen. Es ist das einzige Panel
  eines aktiven Werkzeugs (ToolPanelHost aus ADR 0003).
- **Timeline** unten: alle Features in Reihenfolge. Ein Klick öffnet das Feature zum
  Bearbeiten. Rückrollmarke und Fehlerzustände folgen in M15.
- **Parameter:** rechtes Seitenpanel mit den Dokumentparametern
  ([Konzept](../parameter-table.md)). Maßfelder nehmen `=Name` an.
- **Gestaltung:**
  - Neutrale Flächen mit einer Akzentfarbe (Schmiedeglut-Orange) für Auswahl und
    Hauptaktion.
  - Achsfarben für die Ebenen.
  - Albert Sans für die Oberfläche, JetBrains Mono für Werte.
  - Helle und dunkle Darstellung.
- **Sprache:** Deutsch als Oberflächensprache. Alle Texte laufen über `tr()`, damit
  eine Übersetzung später möglich bleibt. Die Quelltexte sind deutsch (U03b); Tests
  finden Bedienelemente über Command-IDs und Objektnamen, nie über Text.

### Vollparametrik ist nicht verhandelbar

Die einfache Oberfläche ist eine Darstellungsfrage, keine Einschränkung des Modells:

- Jeder Schritt bleibt in der Historie und lässt sich später bearbeiten (Timeline und
  Baum). Es gibt keine direkte Modellierung ohne Historie.
- Skizzen bleiben vollständig über Constraints bestimmbar; der Freiheitsgrad ist
  jederzeit sichtbar.
- Maße lassen sich an Parameter binden. Änderungen laufen in einer Transaktion durch das
  ganze Modell.
- Referenzen folgen ADR 0009. Fehler und Reparatur werden am betroffenen Feature
  angezeigt.
- Kein Befehl ist nur über die Kontextleiste erreichbar. Werkzeugleiste, Befehlssuche
  und Tastenkürzel führen zu denselben registrierten Befehlen. Erweiterte Optionen eines
  Werkzeugs stehen im Panel, notfalls eingeklappt, aber nie weggelassen.

## Alternativen

- **Ribbon mit Arbeitsbereichs-Tabs** (UI-Vorschau, Fusion-ähnlich). Verworfen: viele
  gleichzeitig sichtbare, teils funktionslose Knöpfe. Der Nutzer empfand die bestehende
  Umsetzung als Flickwerk.
- **Nur Kontextleiste ohne Werkzeugleiste** (Shapr3D-ähnlich). Verworfen: Der Nutzer will
  sehen, was es gibt. Die Werkzeugleiste ist ausblendbar und deckt das ab.
- **Feature-Baum ohne Timeline.** Verworfen: Die Timeline macht die Reihenfolge der
  Historie sichtbar und dient als Bearbeitungseinstieg.

## Konsequenzen

- `voelund-ui-preview` ist keine Gestaltungsgrundlage mehr. Ihre Fake-Daten werden nicht
  übernommen. Nachtrag 04.10.2026: U12a hat die Vorschau vorgezogen entfernt, damit keine
  zweite Oberfläche Fehler verdeckt (Nutzerentscheidung).
- U01–U06, U11, U12 und S13 sind auf diesen Aufbau ausgerichtet:
  - Statt des Ribbons bindet U04 Werkzeugleiste, Kontextleiste und Befehlssuche an die
    CommandRegistry.
  - U06 bringt den Baum mit Ursprüngen, Augen-Schaltern, Raster und der Timeline als
    Projektion.
- Die Basisebenen aus V01b (Viewport-Ebenen ohne Baum) werden in U06 an Ursprünge im
  Baum gebunden. Der UI-Test V01c läuft in der neuen Oberfläche.
- Sichtbare Umsetzungen folgen diesem ADR. Aufnahmen werden dem Nutzer weiter zur
  Durchsicht vorgelegt. Abweichungen vom Prototyp werden vorher abgestimmt.
- Prototyp: Design-Artefakt „Völund UI-Konzept“ (Quelle `artifacts/ux01-mockup/`, nicht
  versioniert).
