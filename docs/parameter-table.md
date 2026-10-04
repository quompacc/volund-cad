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
   besteht aus Name, Wert, Einheit (Länge in mm oder Winkel in Grad) und optionalem
   Kommentar.
2. **Stabile Identität.** Jeder Parameter hat eine interne `ParameterId`, die vom Namen
   unabhängig ist. Umbenennen erhält alle Verknüpfungen.
3. **Namen.** Buchstabe am Anfang, danach Buchstaben, Ziffern oder `_`. Eindeutig ohne
   Rücksicht auf Groß-/Kleinschreibung. Keine Kollision mit Einheiten oder Schlüsselwörtern.
4. **Bindung.** Ein Bemaßungs-Constraint (Länge, Abstand, Radius, Durchmesser, Winkel)
   und die Extrusionsdistanz speichern entweder einen festen Wert oder einen Verweis auf
   eine `ParameterId`. Die Einheit muss passen; ein Winkelparameter an einer Länge wird
   abgewiesen.
5. **Änderung.** Eine Wertänderung in der Tabelle ist **eine** Transaktion mit einem
   Undo-Schritt. Betroffene Skizzen werden neu gelöst, abhängige Features neu berechnet.
   Wird eine Skizze dadurch unlösbar, gilt das bestehende Fehlermodell (`committedWithErrors`
   mit sichtbarem Fehlerstatus); nichts wird still verworfen.
6. **Löschen.** Ein noch verwendeter Parameter wird nicht still gelöscht. Angeboten
   werden: Verwendungen auf den aktuellen festen Wert umstellen oder abbrechen.
7. **Lösen der Bindung.** Ein gebundenes Maß kann jederzeit auf seinen aktuellen festen
   Wert umgestellt werden.
8. **Persistenz.** Die Tabelle und die Bindungen werden im nativen Schema gespeichert
   (Schemaerhöhung). Ältere Dateien öffnen ohne Parameter.

## Ausbaustufen

- **v1 (PAR01/PAR02):** feste Werte mit Einheit, Bindung per Name, Umbenennen,
  Löschschutz.
- **v2 (später, eigenes Paket):** einfache Ausdrücke wie `A * 2` oder `B + 5 mm`, mit
  Zyklenprüfung. Erst wenn v1 im Alltag trägt.

## Bedienung (PAR02, Gestaltung im UI-Konzept UX01 abstimmen)

- Tabelle über einen festen Knopf oder ein Tastenkürzel erreichbar, als andockbares Panel.
  Einträge werden direkt in der Zeile bearbeitet.
- Im Maßfeld einer Skizze oder Extrusion statt einer Zahl `=A` eingeben, mit
  Autovervollständigung der Parameternamen. Ein noch unbekannter Name kann direkt als neuer
  Parameter angelegt werden.
- Gebundene Maße zeigen im Viewport und im Panel `A = 10 mm`. Die Tabelle zeigt je
  Parameter die Anzahl der Verwendungen.

## Zeitpunkt

PAR01 folgt direkt auf V01. Der Kern hängt nicht an der Oberfläche. Wird er früh
eingeführt, werden Maße von Anfang an bindbar gespeichert, und spätere
Dateimigrationen entfallen. PAR02 folgt mit der neuen Oberfläche nach UX01 und mit der
vereinheitlichten Dimensionsbindung aus S07.
