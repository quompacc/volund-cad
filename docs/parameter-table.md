# Parametertabelle – Konzept

Stand 04.10.2026. Nutzeridee vom selben Tag; umgesetzt in den Paketen PAR01 (Kern) und
PAR02 (Oberfläche), siehe `implementation-plan/packets.json`.

## Ziel

Wiederkehrende Maße werden nicht als feste Zahlen in Skizzen eingetragen, sondern über
benannte Parameter: `A = 10 mm`. Ein Maß in einer Skizze verweist auf `A`. Ändert man `A`
in der Tabelle auf 20 mm, übernehmen alle verknüpften Maße den neuen Wert und das Modell
rechnet neu. Die Tabelle ist mit einem Klick erreichbar und bewusst einfach. Sie ist
keine Tabellenkalkulation wie FreeCAD-Spreadsheet oder VarSet.

## Fachliche Regeln

1. **Dokumentparameter.** Jedes Dokument hat genau eine Parametertabelle. Ein Eintrag
   besteht aus Name, Wert, Einheit (Länge in mm, Winkel in Grad oder reine Zahl, N01) und
   optionalem Kommentar. Statt eines festen Werts kann ein Ausdruck stehen (N01).
2. **Stabile Identität.** Jeder Parameter hat eine interne `ParameterId`, die vom Namen
   unabhängig ist. Umbenennen erhält alle Verknüpfungen.
3. **Namen.** Buchstabe am Anfang, danach Buchstaben, Ziffern oder `_`. Eindeutig ohne
   Rücksicht auf Groß-/Kleinschreibung. Keine Kollision mit Einheiten oder Schlüsselwörtern.
4. **Bindung.** Ein Bemaßungs-Constraint (Länge, Abstand, Radius, Durchmesser, Winkel)
   und die Extrusionsdistanz speichern entweder einen festen Wert oder einen Verweis auf
   eine `ParameterId`. Die Einheit muss passen; ein Winkelparameter an einer Länge wird
   abgewiesen.
   Alle übrigen Maßfelder (N01b, Nutzerentscheidung vom 21.10.2026) stehen in **einer**
   Bindungsliste neben der Tabelle: Feature, Feldname und Parameter. Dazu gehören
   - Bohrung: x, y, Durchmesser, Tiefe;
   - Verrundung: Radius; Fase: Abstand;
   - Muster: Anzahl, Abstand, Winkel;
   - Grundkörper: Maße und Lage;
   - Drehen: Winkel; Wendel: Steigung;
   - Wandstärke: Dicke; Formschräge: Winkel;
   - Bezugsebene: Abstand, Winkel;
   - Bewegen: Verschiebung und Drehung.
   Fällt ein Feature oder Feld weg, geht seine Bindung mit.
5. **Änderung.** Eine Wertänderung in der Tabelle ist **eine** Transaktion mit einem
   Undo-Schritt. Betroffene Skizzen werden neu gelöst, abhängige Features neu berechnet.
   Wird eine Skizze dadurch unlösbar, gilt das bestehende Fehlermodell (`committedWithErrors`
   mit sichtbarem Fehlerstatus); nichts wird still verworfen.
6. **Löschen.** Ein noch verwendeter Parameter wird nicht still gelöscht. Angeboten
   werden: Verwendungen auf den aktuellen festen Wert umstellen oder abbrechen.
7. **Lösen der Bindung.** Ein gebundenes Maß kann jederzeit auf seinen aktuellen festen
   Wert umgestellt werden. Wird ein gebundenes Feld der Bindungsliste selbst geändert, ohne
   dass sich sein Parameter ändert, löst das die Bindung, und der neue Wert bleibt (N01b).
8. **Persistenz.** Die Tabelle und die Bindungen werden im nativen Schema gespeichert
   (Schemaerhöhung). Ältere Dateien öffnen ohne Parameter.

## Ausbaustufen

- **v1 (PAR01/PAR02):** feste Werte mit Einheit, Bindung per Name, Umbenennen,
  Löschschutz.
- **v2 (N01, Schema 37):** Ausdrücke wie `A * 2` oder `B + 5 mm` mit Funktionen und
  Einheiten, Zyklenprüfung; Bindung aller Maßfelder über die Bindungsliste (N01b).

## Bedienung (PAR02, Nutzerentscheidung vom 16.10.2026)

- Die Tabelle ist eine Karte **links unter dem Modellbaum**. Rechts bleibt Platz für die
  Werkzeug-Panels.
- Man öffnet und schließt sie mit dem Knopf „Parameter“ in der Kopfzeile (neben der
  Befehlssuche), mit **Strg+P** oder über die Befehlssuche (`view.parameters`).
- Die Zeilen zeigen Name, Wert mit Einheit und die Zahl der Verwendungen.
  - Name und Wert werden direkt in der Zeile bearbeitet.
  - „+ Länge“ und „+ Winkel“ legen einen Parameter an (`P1`, `P2`, …).
  - „Entfernen“ (oder Entf) löscht ihn. Ist er in Gebrauch, fragt Völund: „Lösen und
    entfernen“ (die Maße behalten ihren Wert als feste Zahl) oder Abbrechen.
- Solange ein Werkzeug offen ist, bleibt die Tabelle fest.
- Statt einer Zahl `=A` eingeben, mit Vorschlägen der passenden Namen. Das geht in diesen
  Feldern:
  - im Wertedialog eines Skizzenmaßes;
  - in den Feldern „Linienlänge“ und „Kreis-/Bogenradius“ der gewählten Skizzengeometrie;
  - in „Distanz“ und „Versatz“ der Extrusion;
  - (N01b) in allen Zahlenfeldern der Werkzeuge Bohrung, Verrunden, Fase, Muster,
    Wandstärke, Formschräge, Bezugsebene, Bewegen, Drücken/Ziehen, Drehen und Wendel;
  - in den Maß- und Lagefeldern der Grundkörper und der Extrusionsdistanz im
    Eigenschaftspanel. Dort bindet `=A` sofort, als eigener Rückgängig-Schritt.
- Ein noch unbekannter Name führt zur Frage „Den Parameter „A“ = 20 mm anlegen?“. Ja legt ihn
  mit dem bisherigen Feldwert an und bindet das Feld.
  - Das Anlegen ist ein eigener Rückgängig-Schritt.
  - Die Bindung gehört zum OK des Werkzeugs.
- Gebundene Maße zeigen im Viewport und im Feld `A = 20.000 mm`. Eine Zahl im Feld löst die
  Bindung (Regel 7).

## Geltungsbereich

Parameter gelten je Dokument (Nutzerentscheidung vom 04.10.2026). Eine
dateiübergreifende Parameterbibliothek ist vorerst nicht vorgesehen.

## Zeitpunkt

PAR01 folgt auf V01, das nach der neuen Oberfläche (U01–U06) abschließt. Der Kern hängt nicht an der Oberfläche. Wird er früh
eingeführt, werden Maße von Anfang an bindbar gespeichert, und spätere
Dateimigrationen entfallen. PAR02 folgt mit der neuen Oberfläche nach UX01 und mit der
vereinheitlichten Dimensionsbindung aus S07.
