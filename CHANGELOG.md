# Changelog

Hier stehen nachvollziehbare Änderungen für Anwender und Entwickler. Die Gliederung
orientiert sich an [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versionsregeln: [Versionierung und Releases](docs/versioning.md).

## [Unreleased]

Entwicklungsstand vom 16. September 2026. Basisversion: `0.1.0`; noch kein
versionierter Release-Tag. Dieser erste Eintrag fasst den jüngsten D10-/D11-Block
zusammen; frühere Änderungen stehen in Git und den Paketnachweisen.

### Hinzugefügt

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
