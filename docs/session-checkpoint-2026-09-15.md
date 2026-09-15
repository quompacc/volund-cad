# Fortsetzung – 15. September 2026

## Neuester Stand: D10 abgeschlossen, D11a–D11e geprüft

Der Nutzer hat ausdrücklich **„D10 abschließen, D11 beginnen“** gewählt.
Die früheren Offen-Anweisungen zu D10 weiter unten sind damit historisch.
Manifest: D10 `done`, D11 `in-progress`, beide ohne neuen Commit.

[D11a](implementation-plan/evidence/D11a.md) implementiert DocumentId/BodyId-Typen
und eine reine Legacy-Körperzuordnung anhand stabiler Feature-/Supportreferenzen.
Alle 16 Legacyfixtures, mehrere Körper, umgeordnete Datenpaare, ungültige IDs,
Zyklen und eine Kette mit 20.000 Features sind geprüft. Keine Integration in
ModelDocument/FeatureGraph und kein neuer Writer in diesem Teilschritt.

[D11b](implementation-plan/evidence/D11b.md) hält den Feature-Vergabezähler bei
Undo/Redo monoton. Abgebrochene Kandidaten verbrauchen keine IDs. Unveränderliche
DocumentSnapshots unterscheiden Inhaltsgleichheit von vollständigem Speicher-
zustand; der Dirty-Status bleibt bei Undo zur Save-Baseline korrekt. Ein Save
schreibt den erhöhten Zähler über das vorhandene NEXT_ID-Feld. Dokument- und
Körper-ID-Integration war damit noch nicht umgesetzt.

[D11c](implementation-plan/evidence/D11c.md) bindet BodyId an EvaluatedBody und
EvaluatedFeature an. Körper-ID und letzter Shape-Erzeuger sind getrennt;
Join/Cut/Intersect und Extrusion behalten die Körper-ID. Für die vorhandenen
Featurearten ist die Herkunft aus den gespeicherten Feature-IDs/Modi vollständig
rekonstruierbar. Separate reaktiviert die eigene reservierte Zylinder-Herkunft.
Save-As/Open, Moduswechsel, unabhängige umgeordnete Körper und Mehrfachextrusionen
sind geprüft. Historische Shape-Inputs bleiben ausdrücklich R01 vorbehalten.

- FreeBSD/Sway **52/52**, Windows **56/56**.
- D11c: **260 Quellhashes** identisch, Source-Limits 243 Dateien, Fixtures unverändert.
- Alle Prüfläufe beendet. Uncommittet auf `main`, HEAD weiterhin `e8b65b8`.
- Keine Terra-Übergabe; Meridian bleibt L07.

[D11d](implementation-plan/evidence/D11d.md) ergänzt den kanonischen 32-stelligen
Hexcodec für DocumentId und eine native Identitätsprobe. Windows und FreeBSD
bestehen je **8/8 Probetests**, beide Speicherzyklen über OCCT 8.0.1/7.9.3 sind
geprüft. Produktsuiten erneut **56/56 Windows**, **52/52 FreeBSD/Sway**;
**268 Quellhashes** identisch. Alle Builds und Tests beendet.

Der schlanke OCCT-7.9.3-Build enthält jetzt TKBin samt Unterbau. Achtung:
`.tools/occt-freebsd` im isolierten FreeBSD-Prüfbaum verweist auf den ursprünglichen
Checkout. Deshalb wurde ausschließlich unter
`/home/eduard/src/voelund-p04-20260913/build/d11d-occt` gebaut/installiert;
Quellen wurden aus dem gepinnten ursprünglichen Checkout nur gelesen. Produkt und
Proben sind explizit auf diesen neuen Prefix und dessen OpenCASCADE_DIR konfiguriert.
Alle 48 erfassten OCCT-Bibliotheksauflösungen zeigen dorthin. Die gemeinsame alte
Installation wurde nicht überschrieben. Prüfskript und Logs stehen in D11d-artifacts.

[D11e](implementation-plan/evidence/D11e.md) implementiert `generateDocumentId`
mit Betriebssystemzufall (Windows BCrypt, FreeBSD arc4random_buf; Linux getentropy
vorhanden, nicht abgenommen). Keine UUID-Bits, keine Uhr-/Zähler-Fallbacks; Null
und beim Duplikat die Quell-ID werden bis zu 16 Versuche lang übersprungen.
Fehler brechen ab. Der private Generatorhelfer erlaubt deterministische Tests.

`IdentifiedDocumentSnapshot` ist ein neuer unveränderlicher Store-Übergabewert.
`create` vergibt eine ID; Wertkopie/`restore`/`revised` erhalten sie. `duplicate`
vergibt eine neue ID bei unverändertem Snapshot und lokalen Körper-/Feature-IDs.
`revised` weist eine sinkende Vergabemarke ab. Dies ist keine zweite veränderliche
Dokumentquelle und noch nicht in `ModelDocument`/`CadSession`/Writer eingebunden.
VOLUND 16 bleibt unverändert. **58/58 Windows**, **54/54 FreeBSD/Sway**, **275
Quellhashes** identisch; alle Prüfprozesse beendet. Keine Commit-/Branchänderung.

Nächster autorisierter Auftrag ist die D11-Fortsetzung: dauerhafte DocumentId,
explizite Duplikatsemantik und nativen Identitäts-/Schemaübergang gemäß ADR 0008
umsetzen. OCAF-Build/Feldtransport sind durch D11d, Generator und unveränderlicher
Identitätsübergang durch D11e vorbereitet. Als nächstes native Schema-/Store-
und Autoritätsintegration gemäß D12/D13 zusammenführen; keinen Legacy-Writer
einführen, der die neue Dokumentidentität verliert. Produktiver Duplikatbefehl
und dauerhafte Session-Identität fehlen weiterhin. Keine Mischung mit System-OCAF.
Körper-IDs sind im bestehenden Legacy-Pfad
bereits produktiv rekonstruierbar; keine unnötige unabhängige Vergabe einführen,
die der Writer verliert. Nichtwiederverwendung für Feature-IDs ist in D11b geprüft;
verworfene ungespeicherte Sitzungen sind kein Dauerhaftigkeitsnachweis.
Details stehen in D11a–D11e. D10e/D11a/D11b/D11c/D11d/D11e nicht neu beginnen und
D11 nicht als insgesamt abgeschlossen behandeln. Für diesen bereits erteilten
D11-Auftrag keine erneute allgemeine Fortsetzungsfrage stellen.

## Bisheriger Verlauf

Ausgangspunkt: `main`, sauberer Stand `e8b65b8`. Die Arbeit setzt D10b2 fort;
D07/D08/D09/D10a/D10b1/D10b2a wurden nicht neu begonnen. Kein Terra-Einsatz.

## Aktueller Stand

D10b2b ist **abgenommen**: FreeBSD/Sway **46/46**, Windows **50/50** nach
sauberem Neubau. Damit ist D10b2 abgeschlossen.
Der vollständige [Nachweis](implementation-plan/evidence/D10b2b.md) beschreibt
Vertrag, Migration, GUI-Tests und Build-Reproduzierbarkeit.

- Placement-Rotationen und treibende Winkelconstraints sind intern typisierte
  Radiantwerte. Der Application-Editor hat getrennte typisierte Winkelzugriffe.
- Legacy 1–16 und Writer 16 behalten Grad für Placement und Winkelconstraints;
  Arc/Ellipse bleiben wie zuvor Radiant. Keine neue Dateiversion.
- Properties, Manipulator und Skizzen-Winkeldialog bleiben gradwertige
  Eingabe-/Darstellungsgrenzen. Vorzeichen, ganze Placement-Umdrehungen,
  gerichtete Linienwinkel und Undo/Redo sind Teil der Regressionen.
- Keine automatische Ebenenkorrektur, kein OCAF-Produktwechsel, kein
  Funktionsausbau. D10 bleibt offen; der spätere D10d-Nachweis unten enthält die geprüfte A-NUM-01-Matrix.

## Fortsetzung D10c/D10d – erledigte Arbeit

D10c und anschließend D10d wurden ohne Agentenübergabe umgesetzt und geprüft.
D10c: **47/47 FreeBSD/Sway**, **51/51 Windows**.
D10d-Endstand: **48/48 FreeBSD/Sway**, **52/52 Windows**.
[Nachweis D10c](implementation-plan/evidence/D10c.md),
[Nachweis D10d mit A-NUM-01-Matrix](implementation-plan/evidence/D10d.md).

- Gemeinsame Größenlimits: Längen [0,001; 1.000.000] mm, Koordinaten betragsmäßig
  höchstens 1.000.000 mm. Ungültige Altmaße werden mit Bereichsdiagnose abgewiesen.
- Getrennte Toleranztypen; Profilverbindung mit OCCT-Header-Minimum und relativem
  Koordinatenanteil. Linien und Bögen verwenden denselben Wire-Pfad; ursprüngliche
  Kurven bleiben erhalten. Leere Editorvorschauen bleiben erlaubt.
- Extrusion beginnt exakt auf der Supportebene; `joinOverlap = 1e-4` entfällt.
  Vier wiederholte Extrusionen pro Maßstab prüfen Volumen, Bounds, Roundtrip und History.
- 250 übertragene Dateien im Endstand per Hash identisch. Beide Plattformen sauber
  für D10c neu gebaut, Korrekturen/D10d nach explizitem Erneuern der übertragenen
  Dateizeitstempel kompiliert. Alle vollständigen Endläufe regulär beendet.
- Die automatische Freigabeprüfung blockierte zunächst den SCP-Transfer. Der Nutzer
  hat daraufhin den benannten FreeBSD-Quelltransfer und die Builds/Tests ausdrücklich
  mit „ja du darfst“ erneut freigegeben. Kein Terra-Einsatz.

## Historische Abschlussprüfung D10 – anschließend in D10e korrigiert

Der Nutzer wählte ausdrücklich „D10 abschließend prüfen“.
[Prüfbericht](implementation-plan/evidence/D10-review.md):

- Korrigiert: Richtungsabstände X/Y akzeptieren keine Support-Zusatzdaten mehr;
  negativer Regressionstest vor der Korrektur fehlgeschlagen, danach bestanden.
- Endläufe erneut **48/48 FreeBSD/Sway**, **52/52 Windows**, 250 Quellhashes identisch.
- **Offen und dynamisch reproduziert:** `confirmRectangle` erhöht vier Element-IDs
  ungeprüft. Bei `nextId = UINT64_MAX - 1` entstehen ID 0 und ein auf 2 umgelaufener
  Zähler; Bestätigung meldet Erfolg und verändert den Entwurf. Der isolierte
  Diagnosecode und sein Windows-Protokoll sind im Prüfbericht gesichert.
- Die frühere A-NUM-01-Matrix ließ ID-Überlauf aus. D10 ist deshalb fachlich noch
  nicht abnahmefähig, unabhängig vom beibehaltenen Nutzerwunsch zum offenen Status.

## D10e – ID-Grenzen umgesetzt und geprüft

Der Nutzer beauftragte mit „weiter“ die Behebung des ID-Überlaufs.
[Nachweis](implementation-plan/evidence/D10e.md): gemeinsame `IdCapacity`-
Prüfung für Dokument und Editor, vollständiger Bedarf vor Mutation bei
Rechteck/Slot/Polygon, Split/Fillet/Offset/Mirror/Mustern sowie Relationen.
Bestehende Dimensionen benötigen keine neue ID. Null und UINT64_MAX bleiben
reserviert; resultierende nächste Zähler müssen mit dem Legacy-Reader lesbar sein.

Endstand: **49/49 FreeBSD/Sway**, **53/53 Windows**, 253 Quellhashes identisch.
Der neue Integrationstest prüft Ablehnung ohne Entwurfs-/Auswahl-/Historyänderung,
exakt ausreichenden Restplatz, Save/Open und Editor-Undo/Redo. Alle Endläufe beendet.
Die automatische Freigabeprüfung erlaubte den auf 14 geänderte Dateien begrenzten
Transfer, nachdem sie einen Gesamttransfer abgelehnt hatte. Nur geänderte Dateien
wurden auf FreeBSD mit aktualisierten Zeitstempeln neu gebaut.

## Nächster Arbeitsauftrag

Der konkrete ID-Befund aus D10-review ist behoben. Die beschriebenen A-NUM-01-
Fälle sind geprüft. **D10 bleibt auf ausdrücklichen Nutzerauftrag `in-progress`.**
D10e nicht neu beginnen; D11 nicht automatisch vorziehen. Nächster Schritt ist
die weitere Gesamtentscheidung zu D10 beziehungsweise der danach gewählte Auftrag.

Kein Commit/Push. HEAD bleibt `e8b65b8` auf `main`; sämtliche Änderungen liegen
uncommittet im Arbeitsbaum. Kein Terra-Einsatz. Meridian bleibt L07.
Keine Hintergrundfortsetzung eingerichtet.

## Reproduzierbare Builds

Quellen vor dem abnahmerelevanten Build einfrieren. Nach SCP/Tar reichen
übereinstimmende Quellhashes nicht aus: übernommene ältere Dateizeitstempel
können zu neueren, inhaltlich veralteten Objekten passen. Deshalb entweder die
Zeitstempel geänderter übertragener Quellen aktualisieren oder **`--clean-first`**
verwenden. Die D10b2b-Endabnahme verwendet saubere Neubauten beider Plattformen.

Windows: `tools/setup-windows.ps1 -SkipConfigure -PassThru`; dessen
`DeveloperShell` (vcvars64) vor dem CMake-Build aufrufen, im selben Prozesskontext
wie das V:-Mapping. Build `V:/build/p02-qt-final`.
`VOELUND_ANGLE_ARTIFACT_DIR` für Aufnahmen setzen, anschließend
`tools/test-windows.ps1 -BuildDirectory build/p02-qt-final -ReportPath ...`.

FreeBSD: `eduard@192.168.1.238`, Schlüssel
`$env:USERPROFILE/.ssh/meridian_freebsd`, `IdentitiesOnly=yes`, `BatchMode=yes`.
Isolierter Baum `/home/eduard/src/voelund-p04-20260913`, Preset
`freebsd-clang-lean-occt`, eigener Lean-OCCT-Prefix. Das ursprüngliche Checkout
bleibt unverändert. Der Quelltransfer und die Tests wurden in dieser Session
ausdrücklich erneut bestätigt, nachdem die automatische Freigabeprüfung die
Freigabe aus dem alten Checkpoint nicht akzeptierte.

Sway: `env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1
QT_QPA_PLATFORM=wayland`. `VOELUND_ANGLE_ARTIFACT_DIR` für Aufnahmen setzen;
`ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit ...`.

## Git und Abnahme des vorherigen D10b2b-Standes

HEAD bleibt `e8b65b8`; die Änderungen dieser Fortsetzung liegen im Arbeitsbaum.
Kein Commit und kein Push erstellt. 266 übertragene Dateihashes identisch,
228 Quelldateien innerhalb der Source-Limits, Originalfixtures unverändert,
git diff --check sauber. Placement- und Winkeldialog-Aufnahmen beider
Endläufe tatsächlich betrachtet und im Nachweis verlinkt. Alle gestarteten
Builds und Tests sind beendet; keine Hintergrundfortsetzung eingerichtet.
