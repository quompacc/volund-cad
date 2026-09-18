# Fortsetzung – 18. September 2026

**Aktuell: R06 mit R06d-Mehrfachreparatur/Automodus erweitert. Nächster Schritt: R07 (Konturgraph).**
[Gesamtabnahme](implementation-plan/evidence/R06.md),
[UI-/Plattformnachweis](implementation-plan/evidence/R06c.md),
[aktuelle R06d-Erweiterung](implementation-plan/evidence/R06d.md).

## Dauerhafte FreeBSD-Autorisierung

Ausdrückliche Nutzeranweisung vom 18.09.2026: Der BSD-Rechner ist für dieses Projekt
aufgesetzt. Projektdateien einschließlich neuer interner Quelltexte, Tests und
Prüfdaten dürfen dauerhaft nach eduard@192.168.1.238 übertragen werden; Builds,
Tests und Rücktransfer/Auswertung von Artefakten sind autorisiert. Gilt über Pakete,
korrigierte Payloads und Sitzungen hinweg. Keine erneute Einzelbestätigung anfordern.
Verbindlich in [Kapitel 10](implementation-plan/10-autonome-umsetzung.md),
[Hostdokumentation](development-hosts.md) und executionPolicy.freebsdTestAuthorization.
Frühere paketweise Einschränkungen sind dadurch ersetzt.

Prüfbaum /home/eduard/src/voelund-p04-20260913,
Schlüssel C:/Users/eduar/.ssh/meridian_freebsd, IdentitiesOnly/BatchMode.

## Abgenommener Stand R06

R06a: lesende Kandidatenanalyse, keine automatische geometrische Zuordnung.
R06b: persistenter MigrationNeedsRepair-Grund, Schema 4 / Leser 1–4 / BinOcaf 12;
explizite Support-/Kantenreparatur mit Namespace, Dokumentstempel, Originalslot,
aktueller Herkunft, Transaktion, reject/keepEditableErrors und Undo/Redo.
R06c: Aktion Repair references… bei ausgewählter Skizze, fenstermodaler Dialog
mit ursprünglicher Ebene, ungewählten Flächen-/Kantenkandidaten, Vorschau und
lokalen Koordinatenbereichen. Cancel/Refresh/Stale ohne Mutation; Fehlerübernahme
erst nach abgelehntem Versuch und ausdrücklicher Checkbox. Kantenpunkte Start/Mitte/Ende.

A-REF-03 bestanden: einzelner Treffer und zwei echte koplanare Extrusionsflächen
bleiben Vorschläge. Explizite UI-Zuordnung, Cancel, Undo/Redo, Save-As/Open über
Dateidialoge, alte Kantenattachments, unabhängige Zweige und fehlender Support geprüft.
Originaldatei bleibt erhalten. Allgemeine Regionsidentität weiterhin R09.

Windows: **91/91 Tests (121,47 s)**. Danach fünf explizite Indexcasts für Clang,
Windows-Rebuild und **3/3 UI-/Status-/Größentests (9,25 s)**. FreeBSD: Build und
**88/88 Tests (34,97 s)**. 371 identische Quellhashes, vier identische UI-Fixture-
Ausgaben, je drei native Zustände in beide Richtungen geöffnet. 24 UI-Aufnahmen
betrachtet, einschließlich schmalem Dialog und koplanaren Alternativen.
350 Quelldateien im Größencheck. Zehn Quell-/Testdateien gegenüber R06b übertragen.

Alle Nachweise unter evidence/R06c-artifacts. FreeBSD-JUnit kürzt Ausgaben;
r06c-ui-full.log enthält vollständigen UI-Nachlauf mit absolutem Fixtureverzeichnis.
Tests ausschließlich mit absoluten Fixturepfaden starten (Dateidialoge).
Sway kann angeforderte Fenstergrößen ändern; tatsächliche PNG-Größen in ui-review.txt.
Alle Prüf-/Transferprozesse beendet. Kein Commit/Push.

## Nächster Schritt R07

Deterministischen Konturgraphen gemäß 03-fundament.md / packets.json umsetzen.
Voraussetzungen D10 und R01 sind abgenommen. Bestehende SketchProfiles-Auswertung,
SketchGeometry und Tests zuerst lesen; Linien/Arcs/Kreise/Ellipsen/Rechtecke in
orientierte Randstücke überführen. Konstruktion ausschließen, toleranzbewusste
Endpunktverbindungen, Reihenfolge-/Richtungsunabhängigkeit. Offene Enden,
Verzweigungen und Dubletten mit Element-IDs diagnostizieren. Noch keine allgemeine
Schnittbildung/Verschachtelung (R08) oder Regionsidentität (R09).

Der Tagesabschluss-Commit sichert den zusammenhängenden Stand D13d/D14 bis R06d einschließlich KI-/FEM-Planung und Nachweisen.
Planstand: 121 Pakete, **31 done, 89 planned, A02 needs-design**.
90 offen, davon 61 bis G4. KI/FEM gemäß ADR 0010/Kapitel 11; hier kein KI-/FEM-Code.

## R06d – Produktfeedback umgesetzt

Repair all… bietet Mehrfachauswahl mit einem Apply/Undo und Auto repair unique matches.
Nutzer will ausdrücklich sofortige Übernahme eindeutiger Vorschläge; nur ungelöste
Fälle bleiben. Auto startet nur durch den bewussten Klick, nicht durch Open.
Abhängige Supportreparaturen werden topologisch geplant und atomar geprüft.
Mehrdeutigkeit und alte Kantenattachments verbleiben zur Einzelprüfung.
Aktuelle Plattformnachweise: evidence/R06d-artifacts/final-verification.txt.
375 Quellen/Fixtures, 13 Dateien gegenüber R06c; keine Schemaänderung.

Windows **92/92 (124,89 s)**, nach letzter Buttonbeschriftung **3/3 (4,86 s)**;
FreeBSD **89/89 (35,51 s)**. 375 identische Quellhashes, drei identische
semantische Fixture-Ausgaben, reparierte native Batch-Datei in beide Richtungen
geöffnet. Zwölf finale UI-Aufnahmen betrachtet; siehe ui-review.txt für
Darstellungsgrenzen und Plattformdiagnostik.

Produktdurchsicht: Die richtige Anwendung ist
build/p02-qt-final/apps/voelund-cad/voelund-cad.exe (MainWindow mit Repair all…).
Nicht die separate voelund-ui-preview starten. Falscher Preview-Start auf
Nutzerhinweis korrigiert; CAD-Prozess 23096 gestartet.

## Tagesabschluss – 18.09.2026

Nutzer-Rückmeldung zur richtigen CAD-Anwendung: „sehr gut. Scheint zu funktionieren“.
Damit liegt zusätzlich zu den automatisierten Prüfungen eine positive praktische
Rückmeldung zum Reparaturablauf vor. Auf Nutzerwunsch wird der dokumentierte
Stand lokal committet; kein Push. Arbeit für heute beendet, R07 noch nicht begonnen.
Fortsetzung: R07 – deterministischer Konturgraph gemäß Paketplan.
