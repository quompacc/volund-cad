# R06 – Legacy-Referenzen und bewusste Reparatur

18.09.2026. R06 abgeschlossen; Voraussetzungen R05 und D13 abgenommen.
Verbindlich: [Referenzvertrag](../subshape-reference-contract.md), insbesondere
Migrationsregeln und RepairSubshapeReference; A-REF-03 bestanden, siehe [Gesamtabnahme](evidence/R06.md).

## R06a – Originaldaten und Kandidatenanalyse

Status: abgenommen; 89/89 Windows, 86/86 FreeBSD, 363 identische Quellhashes
und vier identische Fixture-Ausgaben. Keine Mutation/Schema- oder UI-Änderung.

Read-only-Prüfung in Features/Application: gespeicherte Ebene, Bindung und alte
Segmentattachments erhalten; benennbare planare Kandidaten aus aktueller vollständiger
Herkunft ableiten. Dokument-ID, Generation/Revision und betroffener Slotkontext
begleiten das Ergebnis. Geometrische Übereinstimmung ist lediglich ein Vorschlag.
Selbst genau ein Normalen-/Abstandstreffer darf keine automatische Migration auslösen.
Mehrdeutige/fehlende/unbelegte Herkunft ist kein speicherbarer Ersatzschlüssel.

[Prüfstand](evidence/R06a.md). Dieser Block ändert weder Import noch Persistenz,
Feature-Auswertung, Editorverhalten oder Quelldateien. Er behauptet keinen bereits
persistenten MigrationNeedsRepair-Zustand und enthält keinen Reparaturbefehl.

## R06b – Persistenter Reparaturzustand und Transaktion

Abgenommen: Windows 90/90, FreeBSD 87/87, 366 identische Quellhashes, beidseitiger nativer Dateiaustausch und sechs betrachtete UI-Aufnahmen. [Nachweis](evidence/R06b.md).

- Native und Text-Altdokumente als Migrationsquellen behandeln. Originalschlüssel,
  alte Support-/Segmentdaten und Migrationsgrund verlustfrei erhalten; Kandidaten
  neu berechnen. Schemaänderung, Snapshot und Native-Validierung gemeinsam prüfen.
- Ohne belegte ursprüngliche Identität MigrationNeedsRepair erhalten, nicht die
  bestehende Legacy-Nächstflächenheuristik als gültigen Bezug veröffentlichen.
  Automatische Übernahme nur mit zusätzlichem eindeutigen Herkunftsnachweis.
- Bewusste Zuordnung mit erwartetem Namespace/Stempel, altem Slotinhalt und
  geprüftem Kandidaten als eine CommandTransaction implementieren. Nachfolgefehler
  folgen der vorhandenen reject/keepEditableErrors-Entscheidung.
- Support-Reparatur und alte Kantenattachments getrennt validieren; keine implizite
  Konvertierung einer Segmentnummer in eine analytische Kantenidentität.
- Undo/Redo stellt alten Fehler bzw. neue Zuordnung wieder her. Save/Open erhält
  auch den unreparierten Zustand; Quelldatei bleibt bis Save unverändert.

## R06c – Produktbedienweg und Gesamtabnahme

Abgenommen: 91/91 Windows, 88/88 FreeBSD, 371 identische Quellhashes, beidseitiger Dateiaustausch und 24 betrachtete UI-Aufnahmen. [Nachweis](evidence/R06c.md).

Kandidaten anzeigen, bewusste Auswahl/Abbruch und diagnostizierbaren Fehlerzustand
anbinden. Eindeutige Altdatei, gleichwertige Kandidaten, veraltete Auswahl,
unabhängige gültige Zweige, Rücknahme und Wiederöffnen prüfen. Windows-/FreeBSD-
Produktsuiten, Dateiaustausch und eigene UI-Sichtprüfung schließen A-REF-03 und R06 ab.

R06c bindet die vorhandenen Application-Befehle an: erst einen mit aktuellem
Dokumentstempel erzeugten Inspektionsstand anzeigen, dann den gewählten
Flächenkandidaten bzw. die benannten Constraint-Slots explizit bestätigen.
Abbruch bleibt ohne Mutation; nach Dokumentänderung Kandidaten neu prüfen.
Teilreparaturen mit verbleibenden Fehlern müssen die bestehende ausdrückliche
Fehlerübernahme verwenden. Keine versteckte Editor-Normalisierung oder Auswahl
des ersten geometrischen Treffers.

Allgemeine Regionsidentität bleibt R09; keine neue Runtime oder KI-/FEM-Implementierung.
Die Teilblöcke zählen nicht als zusätzliche Hauptpakete.

## R06d – Mehrfachreparatur und ausdrücklich gestarteter Automodus

Die Produktdurchsicht ergänzt Repair all… mit Mehrfachauswahl, gemeinsamem
Apply/Undo und Auto repair unique matches. Auf ausdrücklichen Nutzerwunsch
übernimmt der Auto-Klick eindeutige Vorschläge ohne weitere Einzelbestätigung;
nur ungelöste Fälle bleiben sichtbar. Beim Öffnen wird weiterhin nichts
zugeordnet. Mehrdeutige Flächen und alte Kantenattachments bleiben zur Prüfung.
Siehe [R06d-Nachweis](evidence/R06d.md).
