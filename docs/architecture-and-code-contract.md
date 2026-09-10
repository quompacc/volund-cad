# Völund CAD – Architektur- und Codevertrag

**Status:** verbindlich
**Version:** 1.0
**Beschlossen am:** 10. September 2026
**Gültig für:** sämtlichen neu entstehenden Produktivcode, Build-Code und alle Architekturentscheidungen von Völund CAD

## 1. Zweck und Verbindlichkeit

Völund CAD wird als langfristig ausbaufähiges, professionelles parametrisches CAD-System entwickelt. Ziel ist ausdrücklich nicht nur ein kleines mechanisches CAD, sondern die funktionale Breite eines Systems wie FreeCAD mit einem konsistenten, direkt geführten Arbeitsablauf nach dem Vorbild moderner parametrischer CAD-Anwendungen.

Dieser Vertrag ist die verbindliche technische Grundlage des Projekts. Die Begriffe **MUSS**, **DARF NICHT**, **SOLL** und **DARF** sind normativ gemeint:

- **MUSS / DARF NICHT:** zwingende Regel; eine Abweichung blockiert die Integration.
- **SOLL:** Regel, von der nur mit dokumentierter technischer Begründung abgewichen werden darf.
- **DARF:** zulässige Option, aber keine Vorentscheidung für deren Einsatz.

Frühere Machbarkeitsversuche dürfen als Erkenntnisquelle erhalten bleiben, begründen aber keinen Anspruch auf Übernahme ihrer Architektur in das Produkt.

## 2. Verbindliche Technologieentscheidung

Der auszuliefernde CAD-Prozess wird nativ in **C++20** entwickelt.

| Bereich | Verbindliche Technik |
|---|---|
| Build und Abhängigkeiten | CMake mit klar versionierten Abhängigkeiten |
| Desktop-Oberfläche | Qt 6 Widgets |
| Geometrie und Topologie | OpenCASCADE Technology (OCCT) |
| Dokumentmodell, Transaktionen, Persistenz | OCAF und eigene, versionierte Völund-Dokumentstrukturen |
| 3D-Ansicht und Interaktion | OCCT AIS/V3d auf OpenGL-Basis, gekapselt durch Völund-Schnittstellen |
| Linux-Fenstersystem | Wayland nativ; Qt besitzt Oberfläche und Grafik-Kontext |
| Tests | CTest und native C++-Tests |

Folgende Grenzen gelten verbindlich:

1. **Python ist für Produktivcode, Runtime, Build, Tests, Codegenerierung und Release-Prozess verboten.** Vorhandene Python-Artefakte gehören ausschließlich zum historischen Phase-0-Spike und dürfen nicht in den Produkt-Abhängigkeitsgraphen gelangen.
2. **FreeCAD ist keine Runtime-Abhängigkeit.** Das Produkt startet oder lädt weder FreeCAD noch dessen Python-Module, Anwendungskern, GUI, Plug-ins oder ausführbare Programme.
3. **WebView, Electron, Tauri oder eine browserbasierte Hauptoberfläche sind für den CAD-Arbeitsbereich ausgeschlossen.** Eingebettete Webinhalte dürfen später nur für klar isolierte Nebenfunktionen vorgeschlagen werden.
4. **Rust ist nicht Teil des CAD-Prozesses oder seiner ABI.** Rust darf später für einen eigenständigen, prozessisolierten Dienst evaluiert werden, sofern die Kommunikation über ein stabiles, versioniertes Protokoll erfolgt und keine OCCT- oder Qt-Objekte die Prozessgrenze überschreiten.
5. Neue Sprachen, UI-Frameworks oder Kernbibliotheken benötigen vor ihrer Einführung eine genehmigte Architekturentscheidung nach Abschnitt 11.
6. **Wayland ist das verbindliche Linux-Produktziel.** X11 beziehungsweise XWayland darf nur als ausdrücklich markierter, zeitlich begrenzter Diagnoseweg eingesetzt werden und ist kein auszulieferndes Linux-Backend. Die Linux-Ansicht verwendet eine von Qt verwaltete Wayland-Oberfläche und einen von Qt verwalteten OpenGL-/EGL-Kontext; OCCT rendert über eine plattformneutrale Adaptergrenze in diesen Kontext.

Die Python-Artefakte unter `spikes/phase-0-freecad/` sind eingefrorenes
Beweismaterial. Sie dürfen ausgeführt oder gelesen, aber nicht zum Produkt ausgebaut,
vom Produkt referenziert oder als Präzedenzfall für neue Dateien verwendet werden.
Die C++-Produktstruktur schließt den gesamten Ordner `spikes/` im Build ausdrücklich aus.

## 3. Rolle von FreeCAD

FreeCAD dient als **Referenz, Verhaltensorakel und Wissensquelle**, nicht als Unterbau des laufenden Produkts.

Zulässig und erwünscht sind:

- Untersuchung, wie FreeCAD schwierige CAD-Probleme gelöst hat;
- Vergleichstests zwischen Völund und FreeCAD für definierte Eingaben und Ergebnisse;
- Übernahme von Ideen, Algorithmen und Testfällen nach technischer und lizenzrechtlicher Prüfung;
- gezielte Wiederverwendung eines klar abgegrenzten Quellcodebestandteils, wenn dessen Lizenz dies erlaubt und die Herkunft lückenlos dokumentiert ist.

Dabei gelten folgende Pflichten:

- Jede direkte Übernahme oder wesentliche Ableitung nennt Quelldatei, Repository-URL, Commit, Lizenz und Umfang in `docs/provenance/`.
- Fremdcode wird nicht unbemerkt in eigenen Code umformuliert. Lizenzhinweise und Quellpflichten bleiben erhalten.
- Vor jeder Übernahme wird geprüft, ob die jeweilige Datei und alle benötigten Abhängigkeiten kompatibel lizenziert sind.
- FreeCADs Architektur wird nicht blind kopiert. Übernommen wird nur, was zu den Völund-Grenzen passt.
- Proprietäre Gestaltung, Markenzeichen oder geschützte Oberflächenelemente anderer CAD-Produkte werden nicht kopiert. Das Workflow-Ziel beschreibt Bedienprinzipien, keine visuelle Kopie.

## 4. Schichten und Abhängigkeitsrichtung

Der Produktcode wird mindestens in folgende Verantwortungsbereiche getrennt:

```text
desktop (Qt Widgets) ───────┐
                            ├──> application ──> features ──> document ──> kernel
viewport (AIS/V3d/OpenGL) ──┘                        │             │          │
                                                    └─────────────┴──────────┘
platform / adapters: ausschließlich an den äußeren Systemgrenzen
```

Arbeitsbereiche werden zusätzlich als Module hinter einer gemeinsamen Registry
gekapselt. Der Core kennt nur Modul- und Command-Verträge, niemals konkrete
Arbeitsbereiche. `Solid` ist das erste Produktmodul; Surface, Mesh, Sheet Metal und
Assembly bleiben über dieselbe Schnittstelle nachrüstbar. Siehe ADR 0002.

Aktive Werkzeuge verwenden einen generischen temporären Tool-Panel-Lebenszyklus.
Viewport-Griffe und Panel-Felder ändern denselben Preview-Zustand; nur die explizite
Bestätigung schreibt in das Dokument. Abbrechen hinterlässt keinen Undo-Eintrag.
Siehe ADR 0003.

### 4.1 `kernel`

- kapselt OCCT-Geometrie, Topologie, Toleranzen und Modellierungsoperationen;
- fängt OCCT-Ausnahmen an der Modulgrenze ab und übersetzt sie in typisierte Völund-Fehler;
- enthält keine Qt-UI-, FreeCAD-, Python- oder Anwendungslogik.

### 4.2 `document`

- besitzt Identitäten, Parameter, Transaktionen, Undo/Redo, Persistenz und Migration;
- definiert ein versioniertes Dateiformat und atomare Speicheroperationen;
- darf keine Widgets, Dialoge oder Viewport-Zustände kennen.

### 4.3 `features`

- implementiert den parametrischen Feature-Graphen und die Neuberechnung;
- trennt Feature-Definition, Validierung und Ausführung;
- enthält keine UI-Kommandos und keinen direkt sichtbaren Darstellungszustand.

### 4.4 `application`

- implementiert Anwendungsfälle, Commands, Auswahlsemantik und Orchestrierung;
- ist die einzige reguläre Brücke zwischen Oberfläche und Fachmodellen;
- liefert stabile IDs und kleine Datenmodelle statt globaler Zustände.

### 4.5 `viewport`

- besitzt Szene, Kamera, Picking, Vorselektion, Overlays und Darstellungslebenszyklus;
- verändert das Dokument nie direkt, sondern sendet Absichten an `application`;
- darf OCCT-Darstellungstypen nur hinter einer schmalen, dokumentierten Präsentationsschnittstelle verwenden.

### 4.6 `desktop`

- enthält Qt-Widgets, Panels, Dialoge, Aktionen und Layout;
- enthält keine Geometriealgorithmen, Persistenzlogik oder Feature-Berechnung;
- verwendet übersetzbare UI-Texte; Bezeichner und technische Dokumentation im Code sind Englisch.

### 4.7 Allgemeine Abhängigkeitsregeln

- Abhängigkeiten zeigen in Richtung der fachlichen Kernschichten; zyklische Modulabhängigkeiten sind verboten.
- Qt-Typen dürfen nicht in `kernel`, `document` oder `features` gelangen.
- Globale veränderliche Zustände und Service-Locator-Muster sind verboten.
- Ownership, Lebensdauer und Thread-Zugehörigkeit sind an jeder Modulgrenze eindeutig.
- Öffentliche Schnittstellen bleiben klein. Implementierungsdetails werden nicht aus Bequemlichkeit exportiert.
- OCCT-Typen dürfen außerhalb von `kernel` und der definierten Viewport-Grenze nicht unkontrolliert durchs System gereicht werden.

## 5. Verzeichnisvertrag

Die Zielstruktur lautet:

```text
apps/                   ausführbare Programme, dünne Composition Roots
src/kernel/             OCCT-Kapselung und Geometrieoperationen
src/document/           Dokument, Transaktionen, Persistenz, Migration
src/features/           parametrischer Feature-Graph
src/application/        Commands und Anwendungsfälle
src/workspaces/         modulunabhängige Arbeitsbereichs- und Command-Verträge
src/modules/            konkrete, voneinander getrennte Arbeitsbereichsmodule
src/viewport/           AIS/V3d-Darstellung und Interaktion
src/desktop/            Qt-Widgets und Desktop-Integration
src/platform/           Betriebssystem- und Prozessadapter
include/voelund/        bewusst öffentliche C++-Schnittstellen
tests/unit/             schnelle, isolierte Tests
tests/integration/      Modul- und OCCT-Integration
tests/regression/       reproduzierbare Fehlerfälle
tests/differential/     Vergleichsfälle gegen Referenzsysteme
docs/adr/               Architekturentscheidungen
docs/provenance/        Herkunft und Lizenz übernommener Bestandteile
third_party/            unveränderter, versionierter Fremdcode
tools/                  native oder CMake-basierte Projektwerkzeuge
```

Eine neue oberste Produktcode-Struktur darf nur per Architekturentscheidung eingeführt werden. `common`, `utils`, `helpers` und ähnliche Sammelmodule sind verboten; gemeinsam genutzter Code erhält einen Namen nach seiner tatsächlichen Verantwortung.

## 6. Größen- und Komplexitätsgrenzen

Die Grenzen gelten für alle selbst gepflegten C++-, Header-, CMake-, Shader- und UI-Quelldateien. Automatisch erzeugte Dateien und unveränderter Fremdcode sind nur dann ausgenommen, wenn sie eindeutig unter `generated/` beziehungsweise `third_party/` liegen.

| Messgröße | Zielwert | Harte Grenze / Folge |
|---|---:|---|
| Dateilänge | höchstens 400 physische Zeilen | **600 physische Zeilen**; darüber schlägt CI fehl |
| Funktionslänge | höchstens 40 physische Zeilen | 80 Zeilen; darüber ist Aufteilung oder Ausnahmebeschluss nötig |
| Zyklomatische Komplexität | höchstens 8 | 10; darüber schlägt die Qualitätsprüfung fehl |
| Verschachtelungstiefe | höchstens 3 | 4; darüber muss der Kontrollfluss vereinfacht werden |
| Funktionsparameter | höchstens 4 | 5; danach Parameterobjekt oder bessere Abstraktion |

Leere Zeilen, Kommentare und Lizenzköpfe zählen bei der Dateilänge mit. So bleibt die tatsächliche Leselänge begrenzt und die Regel kann nicht durch Formatierung umgangen werden. Dateien werden nicht künstlich aufgeteilt: Jede neue Datei braucht eine klar benennbare Verantwortung und eine schmale Schnittstelle.

## 7. Regeln für sauberen C++-Code

- Jede Klasse und jedes Modul hat genau eine klar beschreibbare Hauptverantwortung.
- Ressourcen werden über RAII verwaltet. Besitzende rohe Zeiger sowie manuelles, verstreutes `new`/`delete` sind verboten.
- `const`, Wertsemantik und unveränderliche Daten werden bevorzugt. Ownership wird mit Werten, Referenzen und passenden Smart Pointern ausgedrückt.
- Nicht dokumentierte magische Zahlen sind verboten. Geometrische Toleranzen, Einheiten und Genauigkeitsregeln werden zentral typisiert oder benannt.
- Narrowing Conversions, undefiniertes Verhalten und ungeprüfte Indexzugriffe werden vermieden beziehungsweise an Grenzen abgesichert.
- Fehler werden typisiert und mit Kontext weitergegeben. Fehler dürfen nicht still geschluckt werden.
- Exceptions überschreiten keine Prozess- oder Plug-in-ABI-Grenze. OCCT-Exceptions werden an der Kernel-Grenze übersetzt.
- Nebenläufigkeit wird ausdrücklich entworfen. UI-Zugriffe erfolgen im UI-Thread; Thread-Sicherheit und OCCT-Kontext werden dokumentiert und getestet.
- Formatierung erfolgt automatisiert. Compiler-Warnungen werden auf hohem Niveau aktiviert und für eigenen Code als Fehler behandelt.
- Auskommentierter Code wird gelöscht. Versionsverwaltung ist die Historie.

## 8. Kommentare und Dokumentation

Code wird so strukturiert und benannt, dass er ohne erklärende Übersetzung verständlich ist. Kommentare erklären **warum**, nicht **was** die nächste Zeile tut.

Dokumentiert werden müssen:

- öffentliche Schnittstellen mit Zweck, Vorbedingungen, Ergebnis und Fehlerfällen;
- Ownership, Lebensdauer, Einheiten, Toleranzen und Thread-Sicherheit, sobald sie nicht offensichtlich sind;
- mathematische oder geometrische Invarianten und nicht triviale Algorithmen;
- Workarounds mit Ursache, externer Referenz und Entfernungskriterium;
- Herkunft übernommener oder abgeleiteter Lösungsansätze;
- Architekturentscheidungen, Dateiformatänderungen und Migrationen.

`TODO`, `FIXME` und temporäre Abschaltungen enthalten immer eine Issue-ID, einen Verantwortungsgrund und – wenn möglich – ein Entfernungskriterium. Kommentare, die dem Code widersprechen, gelten als Defekt.

## 9. Test- und Qualitätsvertrag

Kein Produktcode gilt als fertig, solange die zugehörige Funktion nicht angemessen automatisiert geprüft ist.

Mindestens erforderlich sind:

- Unit-Tests für Regeln, Parameterprüfung und deterministische Algorithmen;
- Integrationstests für OCCT-Operationen, Dokumenttransaktionen, Undo/Redo und Persistenz;
- Save/Close/Reopen-Tests für jede Änderung des Dokumentformats;
- ein reproduzierbarer Regressionstest für jeden behobenen Defekt;
- Differential- oder Charakterisierungstests gegen FreeCAD, wenn dessen Verhalten als Referenz dient;
- Tests für fehlerhafte und degenerierte Geometrie, nicht nur für den Idealfall;
- stabile Tests ohne Abhängigkeit von Ausführungsreihenfolge, lokaler Sprache oder absolutem Dateipfad.

FreeCAD-basierte Referenzergebnisse werden außerhalb des Völund-Produktbuilds in einer
klar getrennten Untersuchungsumgebung erzeugt, mit Version und Eingaben dokumentiert
und als unveränderliche Fixtures eingecheckt. Der normale Build und die reguläre CI
starten oder laden dafür weder FreeCAD noch Python.

Jede Integration muss automatisiert prüfen:

1. Konfiguration und Build auf den unterstützten Plattformen;
2. Formatierung und statische Analyse;
3. Compiler-Warnungen als Fehler für eigenen Code;
4. alle Unit-, Integrations- und relevanten Regressionstests;
5. Schichtgrenzen und verbotene Abhängigkeiten;
6. die 600-Zeilen-Grenze und die vereinbarten Komplexitätsgrenzen;
7. Lizenzen und Herkunft von Fremdcode;
8. das Verbot von Python und FreeCAD als Produktabhängigkeit.

Die Prüfung aus Punkt 8 scannt die neue Produktstruktur. Der namentlich abgegrenzte,
eingefrorene Phase-0-Bestand wird separat inventarisiert und darf sich nicht vergrößern.

Eine reine Prozentzahl ersetzt keine guten Tests. Für neuen oder geänderten Fachcode wird eine hohe Abdeckung erwartet; sicherheitskritische Persistenz-, Transaktions- und Geometriepfade benötigen explizite Positiv-, Negativ- und Grenzfalltests.

## 9.1 Abhängigkeitsbudget

Der reguläre Produktgraph beginnt mit genau zwei externen direkten Abhängigkeiten:
**OCCT** und **Qt Base**. Die C++-Standardbibliothek wird gegenüber zusätzlichen
Hilfsbibliotheken bevorzugt. Weitere Bibliotheken werden nicht vorsorglich aufgenommen,
sind bei echtem technischem Nutzen aber ausdrücklich zulässig.

Das Abhängigkeitsbudget ist kein Selberbaugebot. Komplexe, fachfremde oder
sicherheitskritische Standardprobleme werden nicht aus Prinzip neu implementiert. Wenn
eine reife, gepflegte Bibliothek die insgesamt einfachere, sicherere und langfristig
günstigere Lösung ist, soll sie nach Prüfung verwendet werden. Bewertet werden die
Gesamtkosten aus Eigenentwicklung, Tests, Wartung, Sicherheitsupdates, Paketgröße und
transitiven Abhängigkeiten – nicht allein die Zahl der Bibliotheken.

Jede neue direkte oder transitive Runtime-Abhängigkeit benötigt einen ADR mit:

- nicht sinnvoll selbst implementierbarem Bedarf;
- Vergleich mindestens einer Alternative einschließlich „nicht aufnehmen“;
- Größe und Zahl zusätzlicher transitiver Abhängigkeiten;
- Lizenz, Wartungszustand, Sicherheits- und Updateauswirkung;
- Auswirkung auf Startzeit, Paketgröße und unterstützte Plattformen;
- klarer Kapselungs- und möglicher Entfernungsstrategie.

Optionale Features externer Bibliotheken sind standardmäßig deaktiviert und werden bei
nachgewiesenem Produktbedarf eingeschaltet. Build-only-Abhängigkeiten werden von
Runtime-Abhängigkeiten getrennt ausgewiesen. Ausgeliefert werden ausschließlich zur
Laufzeit tatsächlich benötigte Bibliotheken und Plug-ins.

## 10. Definition of Done

Eine Änderung ist erst fertig, wenn:

- sie einer klaren Schicht und Verantwortung zugeordnet ist;
- Code, Namen und Schnittstellen reviewbar und verständlich sind;
- alle Größen- und Abhängigkeitsgrenzen eingehalten sind;
- relevante Tests vorliegen und lokal sowie in CI bestehen;
- öffentliche Schnittstellen und nicht offensichtliche Entscheidungen dokumentiert sind;
- Formatierung, statische Analyse und Build ohne Warnungen durchlaufen;
- Persistenzänderungen eine Formatversion und Migration besitzen;
- Fremdcode-Herkunft und Lizenz dokumentiert sind;
- keine provisorische Spike-Abhängigkeit unbemerkt in das Produkt gelangt.

## 11. Änderungen und Ausnahmen

Eine Architekturregel wird nicht stillschweigend umgangen. Änderungen und begründete Ausnahmen benötigen vor der Integration einen Architecture Decision Record unter `docs/adr/` mit mindestens:

- Kontext und konkretem Problem;
- betrachteten Alternativen;
- Entscheidung und Auswirkungen;
- betroffenen Modulen und Abhängigkeiten;
- verantwortlicher Person;
- bei einer Ausnahme: Ablaufdatum oder messbares Entfernungskriterium.

Eine Ausnahme von der 600-Zeilen-Grenze ist nur für nachweislich nicht sinnvoll trennbaren, eigenen Code zulässig, zeitlich befristet und muss eine konkrete Aufteilungsaufgabe enthalten. Generierter und unveränderter Fremdcode benötigt keinen Einzelbeschluss, muss aber klar getrennt bleiben.

## 12. Ablösung der Phase-0-Entscheidung

Die Phase-0-Nachweise belegen ausschließlich, dass FreeCAD und OCCT bestimmte CAD-Fähigkeiten technisch bereitstellen. Die darin verwendete Python- und FreeCAD-Runtime-Architektur ist als Produktentscheidung **aufgehoben**.

Ab Version 1.0 dieses Vertrags gilt:

- Völund wird auf OCCT/OCAF als eigenständiges natives C++-Produkt aufgebaut;
- FreeCAD bleibt externe Referenz und Testorakel;
- vorhandene Python-Spikes werden weder erweitert noch als Ausgangspunkt des Produktcodes verwendet;
- der erste Produktmeilenstein ist ein kleiner vertikaler C++-Schnitt durch Kernel, Dokument, Anwendung, Viewport und Qt-Oberfläche – nicht die sofortige Implementierung der gesamten späteren Produktbreite.

## 13. Annahme

Die Aufnahme dieses Dokuments in das Projekt markiert die technische Annahme der Version 1.0. Jede spätere Änderung erhält eine neue Version, ein Datum und einen zugehörigen ADR. Code-Reviews und CI behandeln die Muss-Regeln dieses Vertrags als Integrationskriterien.
