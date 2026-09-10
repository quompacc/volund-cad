# ADR 0003: Generischer Lebenszyklus für aktive Werkzeuge

**Status:** angenommen
**Datum:** 10. September 2026

## Kontext

CAD-Werkzeuge benötigen unterschiedliche Parameter und Auswahlarten, folgen aber
demselben Bedienablauf: starten, Eingaben sammeln, Vorschau aktualisieren, bestätigen
oder abbrechen. Werkzeugdialoge dürfen nicht einzeln in `MainWindow` entstehen.

## Entscheidung

- Der Desktop stellt einen wiederverwendbaren `ToolPanelHost` bereit.
- Ein Werkzeug liefert Titel, Führungstext, Parameter und Callbacks für Änderungen,
  OK und Cancel.
- Fachspezifische Controller verbinden das Panel mit Application und Viewport.
- Parameteränderungen aktualisieren ausschließlich die Vorschau.
- Erst OK schreibt eine atomare Änderung in das Dokument.
- Cancel und Escape entfernen die Vorschau ohne Dokument- oder Undo-Eintrag.
- Direkte Viewport-Manipulation und Panel-Felder bearbeiten denselben temporären
  Parameterzustand.

## Konsequenzen

Extrusion ist die erste Nutzung dieses Vertrags. Weitere Solid-Werkzeuge und spätere
Arbeitsbereichsmodule verwenden denselben Host, ohne dass dieser ihre Fachlogik
kennen muss. Zusätzliche Parametertypen werden am Host ergänzt, nicht als getrennte
werkzeugspezifische Dialogsysteme.
