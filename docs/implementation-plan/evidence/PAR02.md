# PAR02 – Parametertabelle in der Oberfläche

**Status:** abgenommen am 16.10.2026 nach grünem CI-Lauf 646 (`618f685`); der Nutzer hat die
Oberfläche im installierten Produkt gesichtet („sieht nicht schlecht aus, lassen wir erstmal
so“).

## Entscheidungen (Nutzer, 16.10.2026)

- **Ort:** eine Karte links unter dem Modellbaum, nicht rechts wie im ursprünglichen Konzept.
  Rechts bleibt Platz für die Werkzeug-Panels.
- **Öffnen:** Knopf „Parameter“ in der Kopfzeile neben der Befehlssuche, Strg+P, Befehlssuche
  (`view.parameters`).
- **Unbekannter Name nach „=“:** kurze Frage „Den Parameter „A“ = 20 mm anlegen?“. Ja legt ihn
  mit dem bisherigen Feldwert an und bindet das Feld.

Unverändert aus [parameter-table.md](../../parameter-table.md):
- Zeilen direkt bearbeiten.
- Verwendungen zählen.
- Löschschutz.
- Anzeige „A = 10 mm“.
- Keine Ausdrücke.

## Änderung

- **Anwendung:**
  - `SketchEditorSession::bindDimension` bindet ein Skizzenmaß an einen Parameter seiner
    Einheit oder löst es. Ein Winkelparameter an einer Länge wird abgewiesen; der Wert folgt
    dem Parameter.
  - Eine Zahl auf einem gebundenen Maß löst die Bindung (Regel 7). Das gilt im Wertedialog,
    im Feld „Linienlänge“ und im Feld „Kreisradius“.
  - `acceptParameterTableChange` lässt den Editor nach einem zwischendurch angelegten
    Parameter weiter übernehmen. Nur die Tabelle hat sich dann geändert.
  - `ExtrusionMode` trägt Distanz- und Versatzparameter; eine neue Extrusion nimmt den Wert
    des Parameters.
- **Oberfläche:**
  - `ParameterSpinBox` ist ein Zahlenfeld, das auch `=Name` nimmt, mit Vorschlägen der Namen
    passender Einheit. Gebunden zeigt es „A = 20,000 mm“.
  - `ToolPanelHost` erzeugt solche Felder für bindbare Werte und fragt beim Shell nach
    Parametern (finden oder nach Rückfrage anlegen).
  - Bindbar sind:
    - Distanz und Versatz der Extrusion;
    - Linienlänge und Kreis-/Bogenradius im Skizzeneditor;
    - der Wertedialog jedes Skizzenmaßes. Er ist jetzt ein Textfeld; ein gebundenes Maß
      öffnet mit `=Name`.
  - Die Skizzenbeschriftung zeigt gebundene Maße als `A = 20.000 mm`.
  - `ParameterPanel` und die Karte `parameterCard`:
    - Spalten Name, Wert, Nutzung;
    - „+ Länge“ und „+ Winkel“;
    - „Entfernen“ bzw. Entf mit der Frage „Lösen und entfernen / Abbrechen“;
    - fest, solange ein Werkzeug offen ist.
  - Baumkarte und Parameterkarte teilen sich die linke Spalte.
  - Kopfzeilenknopf mit eigenem Symbol; die Befehlstabelle ist ergänzt.

## Prüfungen

| Szenario | Nachweis |
|---|---|
| Nutzerablauf des Pakets per Klick (Einzelheiten unter der Tabelle) | `desktop.parameter_flow` |
| Bindung im Skizzeneditor und bei neuer Extrusion (Einzelheiten unter der Tabelle) | `application.parameter_binding` |
| Gebundenes Maß ist bearbeitbar (vorher abgewiesen) | `application.sketch_annotations` |
| Wertedialog als Textfeld (Zahl, Begründung bei Ablehnung, Grad) | `desktop.sketch_dimension_edit`, `desktop.angle_inputs` |
| Befehl `view.parameters` immer verfügbar | `desktop.command_inventory` |

**`desktop.parameter_flow`:**

- Rechteck 20 × 10, untere Seite `=A`, linke Seite `=B`, jeweils mit Rückfrage angelegt.
- Extrusion `=C` (8).
- Karte unter dem Baum mit drei Zeilen und je einer Verwendung.
- A in der Tabelle auf 30: Ansicht und Volumen folgen (2400 mm³); Rückgängig in der Kopfzeile
  gibt 1600.
- Umbenennen behält die Verwendung.
- Entfernen von C: Abbrechen behält ihn, „Lösen und entfernen“ lässt die Extrusion bei 8.
- Strg+P als Kürzel.

**`application.parameter_binding`:**

- Länge an A gebunden.
- Winkelparameter abgewiesen.
- Zahl löst die Bindung, auch über das Längenfeld.
- Übernehmen trotz zwischendurch angelegter Parameter; Bindung gespeichert; A ändert die
  gespeicherte Linie.
- Neue Extrusion an C gebunden; C ändert das Volumen; Rückgängig.

## Grenzen

- Den Kommentar eines Parameters zeigt die Tabelle als Tooltip; bearbeiten lässt er sich dort
  noch nicht.
- Weitere Felder (Bohrung, Verrundung, Fase, Muster, Grundkörper) nehmen noch keine Parameter;
  der Kern bindet bisher Skizzenmaße und Extrusionen.
- Das Anlegen eines Parameters aus einem Feld ist ein eigener Rückgängig-Schritt; die Bindung
  gehört zum OK des Werkzeugs.
- Keine Ausdrücke (`A * 2`), wie im Konzept vorgesehen (v2).
