# Changelog

Hier stehen nachvollziehbare Änderungen für Anwender und Entwickler. Die Gliederung
orientiert sich an [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versionsregeln: [Versionierung und Releases](docs/versioning.md).

## [Unreleased]

Entwicklungsstand vom 16. September 2026. Basisversion: `0.1.0`; noch kein
versionierter Release-Tag. Dieser erste Eintrag fasst den jüngsten D10-/D11-Block
zusammen; frühere Änderungen stehen in Git und den Paketnachweisen.

### Hinzugefügt

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
- Aktueller Quellstand: **64/64 Windows**, **60/60 FreeBSD/Sway**; 291 Quellhashes
  identisch. Native D11d-Proben: jeweils **8/8**, beide Austauschrichtungen geprüft.
- Die dauerhafte Dokument-ID ist noch nicht in produktive New-/Open-/Save-As-
  und Duplikatbefehle eingebunden. Der Produktwriter bleibt **VOLUND 16**.
  Der native Autoritäts-/Store-Wechsel und Meridian-Abnahme bleiben offen.
- Details und Testbelege: [D10](docs/implementation-plan/evidence/D10.md),
  [D11k](docs/implementation-plan/evidence/D11k.md),
  [Entwicklungsstand](docs/development-status.md).
