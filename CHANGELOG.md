# Changelog

Hier stehen nachvollziehbare Änderungen für Anwender und Entwickler. Die Gliederung
orientiert sich an [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versionsregeln: [Versionierung und Releases](docs/versioning.md).

## [Unreleased]

### Hinzugefügt

- **STEP:** Dateien lassen sich ins Fenster ziehen. Große Dateien werden im Hintergrund gelesen, mit Fortschritt und Abbrechen. Bis zu 10.000 Volumenkörper je Import.
- **Fläche versetzen und löschen** nehmen jetzt auch gekrümmte Flächen an: Eine Bohrungswand ändert den Durchmesser, eine Ebene nimmt tangentiale Verrundungen mit.

### Behoben

- Linux: Abstürze in OCCT bei Import, Flächenlöschen und Booleschen Operationen auf echten Teilen (OCCT wird jetzt mit `-O2` gebaut).
- Fläche versetzen scheiterte auf echten Teilen fast immer.
- Fläche löschen meldete manchmal Erfolg, ohne etwas zu ändern; das wird jetzt abgelehnt.

## [1.0.0-rc.1] – Releasekandidat, 30. Oktober 2026 (noch nicht veröffentlicht)

Erster Kandidat der v1 für **Windows (x64) und Linux (x86_64)**. FreeBSD/Meridian ist
zurückgestellt (Nutzerentscheidung 21.10.2026). Umfang und Grenzen für Anwender: das
[Handbuch](docs/handbuch.md); Einstieg: das [Tutorial „Referenzplatte“](docs/tutorial-referenzplatte.md);
Prüfprotokoll: [L10-Nachweis](docs/implementation-plan/evidence/L10.md).

### Hinzugefügt (seit dem Stand vom 17. September, gebündelt)

- **Skizzen:** Linie, Polylinie, Rechtecke (Ecke, Mitte, drei Punkte, abgerundet, Rahmen), Kreise,
  Bögen, Ellipse und Ellipsenbogen, Langloch, Vieleck, Punkt, Splines über Kontrollpunkte oder
  durch Punkte, Text; Werte direkt an der Maus; Beziehungen, Normbemaßung mit Parametern und
  Ausdrücken; Stutzen, Verlängern, Teilen, Versetzen, Verrunden/Fasen, Spiegeln und Muster;
  Projizieren, Schnittkurven, DXF-Import.
- **Körper:** Extrudieren (auch mit Schrägungswinkel, bis Fläche, symmetrisch), Drücken/Ziehen,
  Drehen, Folgen, Ausformung, Wendel, Rippe, Grundkörper, Bezugsebenen, -achsen und -punkte.
- **Ändern:** Verrunden, Fasen (drei Arten), Bohrungen (Senkung, Flachsenkung, Spitze,
  ISO-Gewinde als Angabe, an Skizzenpunkten, über zwei Kanten), Wandstärke, Formschräge, Fläche
  versetzen/löschen, Teilen, Spiegeln, Muster (linear, kreisförmig, Raster, entlang Kurve,
  Feature-Muster), Kombinieren, Bewegen, Kopieren/Einfügen.
- **Historie:** Timeline mit Bearbeiten, Unterdrücken, Zurückrollen, Umordnen, Löschen mit
  Abhängigen und Reparatur; Parametertabelle mit Ausdrücken.
- **Prüfen:** Messen (auch an Punkten), Überschneidungen, Material, Masse und Schwerpunkt,
  Schnittansicht, Darstellungsstile und Körperfarben.
- **Austausch:** STEP-Import und -Export mit Farben, IGES, STL, 3MF, DXF.
- **Plattform:** portables ZIP für Windows, Tarball mit Benutzerinstallation für Linux,
  Dateizuordnung, Wiederherstellung nach Absturz, Programmsymbol.

### Bekannte Grenzen

- Keine Baugruppen, Zeichnungen, Blechfunktionen oder Freiformflächen.
- Das Programm ist unsigniert; Windows (Smart App Control) kann es blockieren.
- Siehe „Grenzen“ in jedem Abschnitt des Handbuchs.

## [0.1.0-Entwicklungsstand] – 17. September 2026

Entwicklungsstand vom 17. September 2026. Basisversion: `0.1.0`; noch kein
versionierter Release-Tag. Dieser erste Eintrag fasst den jüngsten D10-/D11-Block
zusammen; frühere Änderungen stehen in Git und den Paketnachweisen.

### Hinzugefügt

- R10 abgeschlossen (V01-Umfang): Regionen verschachtelter Konturen werden gültige Flächen mit Innenwires und analytischem Inhalt (A-REG-01: Ring 300π, Innenregion 100π, Inseln, Permutation, Toleranzmatrix); Fehler nennen die betroffenen Element-IDs. Schneidende Konturen folgen als R10b nach V01. [Nachweis](docs/implementation-plan/evidence/R10.md).

- R09 abgeschlossen (V01-Umfang): dauerhafte Regionsidentität aus kanonischen Randsignaturen statt kleinster Element-ID; Auswahl per Rand-Pick, natives Schema 5 mit Regionsauswahl, Migration alter Profil-IDs beim Öffnen, Ringextrusion mit Loch, Split und gelöschte Ränder werden reparaturbedürftig statt umgedeutet. Offene Skizzenelemente verhindern keine Regionen mehr. A-REG-02 besteht. [Nachweis](docs/implementation-plan/evidence/R09.md).

- C01 abgeschlossen: Pflicht-CI je Push/PR auf dem Fedora-Runner (`gitea-runner`, Label `linux-desktop`). Build, Quellgrenzen und vollständiger CTest in headless `kwin_wayland --virtual` auf der Host-GPU; Logs, JUnit und Aufnahmen als Laufartefakt. Rot-Prüfung und Neustart ohne Desktopanmeldung belegt. [Nachweis](docs/implementation-plan/evidence/C01.md).

- P05 abgeschlossen: Linux-Desktop mit nativem Wayland (Fedora 44, KDE) über Preset `linux-desktop`, projektlokales OCCT 7.9.3 (`tools/build-lean-occt.sh`), `tools/setup-linux.sh` und `tools/run-linux-desktop.sh`. Behoben: nichtdeterministisches Editor-Undo/Redo durch erneutes PlaneGCS-Lösen und in den Toolbar-Überlauf verdrängte Verbindungsauswahl (Modi/Snap jetzt zweite Toolbarzeile). 92/92 Fedora, 95/95 Windows. [Nachweis](docs/implementation-plan/evidence/P05.md).

- R08 abgeschlossen: orientierte Regionen und unmittelbare Innenränder, Ring/Innenfläche getrennt, explizite Tangential-/Offenranddiagnosen. 95/95 Windows, 92/92 FreeBSD, 390 gleiche Quellhashes. Produktanbindung folgt R09/R10. [Nachweis](docs/implementation-plan/evidence/R08.md).

- R08a: analytische Schnittpunkte, Tangentenklassifikation und Quellparameterintervalle mit Herkunft. 94/94 Windows, 91/91 FreeBSD, 385 gleiche Quellhashes. Regionsbildung folgt R08b. [Nachweis](docs/implementation-plan/evidence/R08a.md).

- R07: deterministischer Konturgraph, explizite Laufrichtung und lokale Diagnose offener/verzweigter/überlappender Grenzen. 384 Permutations-/Richtungsvarianten; 93/93 Windows, 90/90 FreeBSD, 380 gleiche Quellhashes. [Nachweis](docs/implementation-plan/evidence/R07.md).

- R06d: dokumentweite Mehrfachauswahl und unmittelbar anwendender Automodus für eindeutige Supportvorschläge. Unklarheiten bleiben offen; ein Undo je Reparaturlauf. [Nachweis](docs/implementation-plan/evidence/R06d.md).

- R06 abgeschlossen: Reparaturdialog mit ausdrücklicher Kandidatenwahl, Vorschau, Cancel, Stale-Prüfung und bestätigter Fehlerübernahme. A-REF-03 bestanden; 91/91 Windows, 88/88 FreeBSD, 371 identische Quellhashes, beidseitiger Dateiaustausch und 24 betrachtete UI-Aufnahmen. [Gesamtabnahme](docs/implementation-plan/evidence/R06.md).

- R06b: persistenter Reparaturzustand in nativem Schema 4 und explizite Support-/Kantenreparatur als Transaktion mit Undo/Redo. 90/90 Windows, 87/87 FreeBSD, 366 identische Quellhashes, beidseitiger Dateiaustausch und sechs betrachtete UI-Aufnahmen. Auswahloberfläche folgt in R06c. [Nachweis](docs/implementation-plan/evidence/R06b.md).

- Dauerhafte projektweite Nutzerfreigabe für Projektdateitransfers und Builds/Tests auf dem dedizierten FreeBSD-Prüfrechner dokumentiert; gilt über Pakete und Sitzungen hinweg ohne erneute Einzelbestätigung.

- R06a: lesende Referenzprüfung mit erhaltenen Originaldaten und eindeutig benannten Reparaturkandidaten. Geometrische Treffer bleiben Vorschläge; kein automatischer Wechsel. 89/89 Windows, 86/86 FreeBSD, 363 identische Quellhashes und vier identische Fixture-Ausgaben. Reparaturzustand und UI folgen separat. [Nachweis](docs/implementation-plan/evidence/R06a.md).

- R05 insgesamt abgenommen: stabile Randkanten im Skizzeneditor, analytische Fangpunkte, Referenzen an beiden Linienenden und Rollback bei abgewiesenem Attachment. 88/88 Windows, 85/85 FreeBSD, 358 identische Quellhashes und beide UI-Aufnahmen geprüft. [Gesamtnachweis](docs/implementation-plan/evidence/R05.md).

- R05b-P plattformübergreifend abgenommen: typisierte projizierte Kantenreferenzen und analytische Constraintauflösung, natives Schema 3 mit Leser für 1/2/3, Namespace-Erhaltung beim Duplizieren. 87/87 Windows-Tests, 84/84 FreeBSD-Tests, 353 identische Quellhashes und beidseitiger nativer Dateiaustausch; UI-Anbindung folgt in R05b-U. [Nachweis](docs/implementation-plan/evidence/R05b-P.md).

- Plan um native CAD-Aktionen, integrierte KI-Bedienung und gestuften FEM-/Optimierungsausbau erweitert. ADR 0010 und K12/K13 sind verbindlich; AI02 liegt vor U04. Keine neue Runtime oder KI-/FEM-Implementierung in diesem Planungsschritt. [Plan](docs/implementation-plan/11-ki-fem-optimierung.md).

- R05b-Konturgrundlage plattformübergreifend geprüft: feste Skizzenelement-/Seitenrollen,
  tatsächliche Drahtkanten und Prism-Anfang/Ende, Kreisrahmen sowie getrennte Welt-/Referenzkanten.
  86/86 Windows, 83/83 FreeBSD, 347 identische Quell-/Fixture-Hashes. Persistente
  Kantenconstraints und UI-Anbindung bleiben offen. [Prüfstand](docs/implementation-plan/evidence/R05b.md).

- R05a plattformübergreifend abgenommen: semantische Primitive-Kantenherkunft, analytische
  Kurvenparameter und tessellierungsunabhängige Auflösung. 85/85 Windows-Tests,
  82/82 FreeBSD-Tests und 344 identische Quell-/Fixture-Hashes. Speicherung und Produktanbindung folgen
  R05b. [Prüfstand](docs/implementation-plan/evidence/R05a.md).

- R04 abgeschlossen: gepickte Flächen werden dauerhaft gebunden; Editor, Überlagerung
  und Extrusion folgen der aktuellen Fläche. Fehler nach Löschung bleiben sichtbar.
  Referenzgeometrie für verschobene/gedrehte Extrusion korrigiert. 84/84 Windows,
  81/81 FreeBSD, 341 identische Quell-/Fixture-Hashes und eigene UI-Sichtprüfung.
  [Gesamtabnahme](docs/implementation-plan/evidence/R04.md).

- R04b als Teilblock abgenommen: dauerhafte Stützflächenbindung mit relativer Basis,
  benannte Extrusions-Endflächen, Schema 2 bei weiter lesbarem Schema 1.
  83/83 Windows, 80/80 FreeBSD, 339 identische Quell-/Fixture-Hashes und
  beidseitiger Dateiaustausch bestanden. [Abnahme](docs/implementation-plan/evidence/R04b.md).

- R04a als Teilblock abgenommen: Herkunft bekannter Stützflächen im Feature-Auswertungsstand, einschließlich
  Transformation, Boolean-Fortführung und Cache. 82/82 Windows, 79/79 FreeBSD; 332 identische Quellhashes. [Abnahme](docs/implementation-plan/evidence/R04a.md).
  Die anschließende Bindung ist in R04b abgenommen; Produktanbindung ist in R04c abgenommen.

- R03 abgeschlossen: Primitive-Rollen und echte Generated/Modified/Deleted-
  Historie für Boolean-Operationen, Zuordnung bei starren Transformationen.
  81/81 Windows, 78/78 FreeBSD; Historienvergleich und 328 Quellhashes identisch. [Prüfstand](docs/implementation-plan/evidence/R03.md).

- R02 als Entscheidungspaket abgeschlossen: typisierte Subshape-Herkunft, Split-/Merge-Regeln,
  Persistenz-/Reparaturentwurf und jeweils 26 symbolische Fixtures auf Windows/FreeBSD.
  [Abnahme und Grenzen](docs/implementation-plan/evidence/R02.md); Produktintegration folgt R03–R06.

- R01 abgeschlossen: expliziter Feature-DAG, gezielte Neuauswertung bei
  Parameteränderungen und editierbare gültige Zweige trotz Fehlern anderer Zweige.
  80/80 Windows, 77/77 FreeBSD/Sway; 325 identische Quell-/Fixture-Hashes.
  [Prüfstand und Grenzen](docs/implementation-plan/evidence/R01.md).

- D14 abgeschlossen: VOLUND 1–16 über natives Speichern, Öffnen, Editieren und
  Undo/Redo geprüft; alle 26 Constraintarten einschließlich alter Winkelkonvention.
  Echte gesperrte Migration erhält Altdatei und Sitzung. 79/79 Windows, 76/76 FreeBSD.
  [Abnahme und offene G1-Arbeiten](docs/implementation-plan/evidence/D14.md).

- D13 abgeschlossen: produktiver Dateiaustausch Windows ↔ FreeBSD mit vollständigen
  Snapshots/IDs, analytischer Geometrieprüfung und reparierbaren Fehlerzuständen.
  77/77 bzw. 74/74 Tests; zusätzliche Abnahme einer fehlenden Profilreferenz.
  [Gesamtnachweis](docs/implementation-plan/evidence/D13d.md).

- Desktop-Aktion **File → Duplicate**: neue ungespeicherte Dokumentkopie; bei
  Änderungen wahlweise Original speichern, ohne Speichern duplizieren oder abbrechen.
  Abbruch und Fehler vor dem Duplizieren erhalten die bisherige Dokumentidentität.

- Dokument-ID und identifizierter Snapshot an CadSession: Bearbeitung, Undo/Redo
  und Save-As behalten die ID; New, Legacy-Open und explizites Anwendungsduplikat
  erzeugen neue Namensräume. D11m mit 66/66 Windows und 62/62 FreeBSD geprüft.
  Die ID wird noch nicht in VOLUND 16 gespeichert; Desktop-Anbindung siehe D11n.

- Gemeinsamer nativer Schematest mit allen vier Featurearten, drei unabhängigen
  Körpern, Umordnung, Umbenennung, Duplikat-/Revisionssemantik und ID-Vergabe nach Undo.
  Die D11l-Prüfung dokumentiert die Grenzen zu Produktidentität und Mehrkörper-UI.

- Native Extrusionen mit Distanz, Skizzeneingabereferenz und geordneter Profilauswahl.
  Körperidentitäten, Folgeextrusionen und historische unbekannte Profil-IDs bleiben
  erhalten. Damit sind alle vier vorhandenen Featurearten im privaten Adapter abgebildet.

- Native Speicherung aller 26 Skizzen-Constraintarten mit Elementreferenzen,
  getrennten Maß-/Radiantwerten und vollständigen Zusatzparametern. Lösbare und
  widersprüchliche Skizzen behalten identische Solver-Ergebnisse nach Save/Open.

- Native Skizzengeometrie mit Ebenen, Element-IDs, Parametern, Konstruktionsflags
  und Stützreferenzen; alle fünf Elementarten samt Vergabezählern verlustfrei.
  Produktiver nativer Dispatch bleibt offen.

- Native Zylinder-Abbildung für Join/Cut/Intersect/Separate mit echten
  Eingabereferenzen, Körpergruppen und eigenständiger Feature-Reihenfolge.
  Beide Plattformen erhalten Snapshots und neu ausgewertete Geometrie identisch.
- Native Box-Abbildung mit expliziten Körper-/Feature-IDs, Namen, Reihenfolge und
  Radiant-Parametern. Snapshot-Parität und rekonstruierte BReps auf beiden Plattformen
  geprüft; nicht unterstützte Featurefamilien werden vor dem Schreiben abgewiesen.
- Privater nativer Dokumentkopf mit Kennung, Schema, Dokument-ID und verlustfreiem
  Feature-Vergabezähler; auf Windows/FreeBSD einschließlich nativem Dateiaustausch
  geprüft, Produktdispatch noch geschlossen.
- Getrennte Dokument- und Körper-ID-Typen; Körperidentität ist in der Auswertung
  vom letzten erzeugenden Feature getrennt und bleibt über weitere Features,
  Moduswechsel sowie Save-As/Open erhalten.
- Betriebssystembasierte Dokument-ID-Erzeugung und ein unveränderlicher
  Snapshot-Übergang mit expliziter Kopie-, Revisions- und Duplikatsemantik.
- Verlustfreie native ID-Feldkodierung und isolierter FreeBSD-OCCT-Build mit OCAF;
  native Probe-Dateien sind zwischen OCCT 7.9.3 und 8.0.1 austauschbar.

### Geändert

- D13c: Save/Open verwendet natives BinOcaf (Container 12, Anwendungsschema 1).
  Dokument-ID bleibt bei nativem Open und Save-As erhalten; VOLUND 1–16 bleiben
  lesbar und erhalten beim Import eine neue ID. AtomicFileStore-Fehlergarantien
  und editierbare Geometriefehler bestehen weiter. 76/76 Windows, 73/73 FreeBSD.
  Alte Programmstände mit reinem Textleser können neue native Dateien nicht öffnen.

- D13b nach Gesamtreview abgeschlossen; Prüfgrenzen und D13c-Einstieg dokumentiert.
  317 Quellhashes erneut bestätigt; vorhandene 75/72-Tests übernommen, nicht neu ausgeführt.
  D13 bleibt offen, VOLUND 16 bleibt bis D13c Produktwriter.

- D13b2 begrenzt native Ausgabebytes bereits während des Schreibens und erhält
  OCCTs Rückwärtskorrekturen von Größen/Offsets. Zusätzliche Grenztests prüfen
  das Referenzpfadbudget und die maximale interne Attribut-ID.
- D13b2 ergänzt die Vorprüfung der fünf unterstützten Attributtreiber:
  innere Werte, Array-/Referenzgrenzen, eindeutige Attribut-IDs und begrenzte
  Referenzpfade. Plattformabnahme und Gesamtabschluss stehen im Paketnachweis.
- D13b2 prüft zusätzlich die BinOcaf-12-Struktur vor OCCT-Open:
  Abschnittstabelle, äußere Attributblockgrößen, Labelanzahl/Baumtiefe und
  exaktes Dateiende. Innere Attributwerte und Treiberzuordnung sind inzwischen ergänzt.
- D13b2 ergänzt eine Vorprüfung des BinOcaf-12-Dateikopfs vor nativem Öffnen:
  Grenzen, Textlängen, Anzahlen und Version werden ohne eingabeabhängige
  Pufferreservierungen geprüft. Native Nutzdatenprüfung ist inzwischen ergänzt.
- D13b1 ergänzt privates, begrenztes Einlesen ohne Seek und eindeutige Zuordnung
  zu Legacy-Text oder nativer Binärcontainerfamilie. Die Containerprüfung ist
  inzwischen abgeschlossen; Produktintegration folgt in D13c. Windows- und FreeBSD/Sway-Abnahme
  einschließlich identischer Quellstände bestanden.
- D13a ergänzt einen privaten BinOcaf-Adapter für unveränderliche Dokument-Snapshots,
  mit ID-/Parameter-Rundlauf, Schemafehlern und Eingabegrößenprüfung vor dem Öffnen.
  Produktdispatch und Writerwechsel bleiben folgende Teilblöcke.
- D12e schließt die Gesamtabnahme der OCAF-Modellautorität ab und ordnet die
  ursprünglichen Kriterien den bestehenden Plattform- und Fehlernachweisen zu.
  Nur Dokumentation; native Produktpersistenz bleibt D13.
- D12d stellt das Produktmodell mit Transaktionen und Undo/Redo auf die private
  OCAF-Autorität um. Modellabfragen nutzen eine unveränderliche abgeleitete Ansicht;
  CadSession übernimmt die Dokument-ID des Modells. VOLUND 16 bleibt Produktwriter.

- D12c ergänzt einen isolierten privaten OCAF-Zustand mit vorbereiteten Änderungen,
  Abbruch und Undo/Redo bei monotonem ID-Vergabestand. Historie hält vollständige
  OCAF-Stände; die Produktanwendung wird erst in D12d umgestellt.

- D12b bindet Geometrieauswertung und Legacy-Writer an je einen eingefrorenen
  Dokumentstand. Die Oberfläche erhält eigene Parameter-/Featurewerte, die nach
  Modellwechsel gültig bleiben. D12c–D12e trennen OCAF-Vorbereitung, Umschaltung
  und Gesamtabnahme. VOLUND 16 bleibt unverändert.

- D12a kapselt Modellzustand und Historie privat. Unabhängige Wertkopien, atomare
  Kopierzuweisung und Transaktionskandidaten sind abgesichert; der OCAF-Backendwechsel
  folgt separat. VOLUND 16 und die Bedienabläufe bleiben erhalten.

- D11 anhand der ursprünglichen Identitätskriterien abgeschlossen und nachgewiesen.
  OCAF-Autorität (D12), native Produktpersistenz (D13) und Mehrkörper-Bedienabnahme
  bleiben offen. D12a ist als erster Kapselungsblock konkretisiert; keine Produktänderung.

- Verbindliches v1-Ziel präzisiert: direkter Mehrkörperworkflow ohne Komponenten-/
  Baugruppenpflicht, individueller Körperbereich und explizites NewBody. Neue
  geplante Abnahme A-BODY-03; dies dokumentiert eine Anforderung, keine fertige Funktion.
- Der private Primitive-Adapter ersetzt den reinen Box-Adapter. Das unveröffentlichte
  native Entwurfslayout heißt jetzt `Primitives.v1`; ältere `Boxes.v1`-Probedateien
  bleiben historische Nachweise. Der produktive Legacy-Reader ist unverändert.
- Interne Winkel verwenden durchgängig Radiant; Anzeige und Legacy-Dateigrenzen
  konvertieren ausdrücklich. Größenlimits und Toleranzen haben getrennte Rollen.
- Extrusionen verwenden den exakten Supportflächenanschluss ohne pauschalen Überstand.

### Behoben

- ID-Überlauf wird vor der Mutation und für den gesamten ID-Bedarf abgefangen.
- Veröffentlichte Feature-IDs werden nach Undo nicht erneut vergeben; der erhöhte
  Vergabezähler bleibt über Save/Open erhalten, ohne den Dirty-Vergleich zu verfälschen.
- Nicht endliche Werte, unzulässige Größen und ungültige Ebenen werden an den
  vorgesehenen Eingabegrenzen erkannt; Legacy-Daten werden nicht still korrigiert.

### Prüfung und offene Integration

- D10 abgeschlossen; D11 insgesamt weiterhin in Arbeit.
- Aktueller Quellstand: **66/66 Windows**, **62/62 FreeBSD/Sway**; 294 Quellhashes
  identisch. Native D11d-Proben: jeweils **8/8**, beide Austauschrichtungen geprüft.
- Die dauerhafte Dokument-ID ist noch nicht in produktive New-/Open-/Save-As-
  und Duplikatbefehle eingebunden. Der Produktwriter bleibt **VOLUND 16**.
  Der native Autoritäts-/Store-Wechsel und Meridian-Abnahme bleiben offen.
- Details und Testbelege: [D10](docs/implementation-plan/evidence/D10.md),
  [D11n](docs/implementation-plan/evidence/D11n.md),
  [Entwicklungsstand](docs/development-status.md).
