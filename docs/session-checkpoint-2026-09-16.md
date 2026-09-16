# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11k geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. Prüfbasis: `407def2` (Pausendoku),
vorheriger Implementierungsstand `cddf45d` (D11j). Die Arbeit wurde auf Nutzerauftrag
wiederaufgenommen. D11k wird mit Belegen, Changelog und dieser Fortsetzungsdoku
auf `main` committed; den Abschlusscommit über `git log -1 --oneline` feststellen.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11k

- Der private `NativePrimitiveCodec` unterstützt alle vier aktuellen Featurearten.
- `NativeExtrusion` speichert positive Distanz und geordnete Profil-IDs.
- Die Eingabeskizze ist eine echte OCAF-Referenz; ihre Art, Zyklen und Körperzuordnung
  werden geprüft. Extrusionen behalten die BodyId ihres Supports.
- Leere Profilauswahl bedeutet weiterhin „alle geschlossenen Profile“.
- Historisch unbekannte Profil-IDs bleiben lesbar und reproduzieren den
  Auswertungsfehler. Profil-IDs sind keine nativen Element-/Labelreferenzen.
- Profil-IDs erlauben gemäß Modell/Legacy-Importer den vollständigen positiven
  uint64-Bereich; nicht mit dem begrenzten Vergabezählerbereich verwechseln.
- Das Gesamtbudget zählt Features, Elemente, Constraints und Profilauswahlen.
- Vorherige private Schemafelder bleiben kompatibel; D11j-Dateien bleiben lesbar.

**64/64 Windows**, **60/60 FreeBSD/Sway**; native Extrusionsdateien in beiden
Richtungen samt Rückschreiben/-lesen bestanden, alle vier Dateien byteidentisch.
**291 Quellhashes identisch**. Alle Builds und Tests beendet.
[Evidenz und Artefakte](implementation-plan/evidence/D11k.md).

## D11l – lokaler Zwischenstand

Auf Basis von `f962bef` wurde die gemeinsame Schema-/Identitätsprüfung durchgeführt.
Alle aktuellen Snapshotfelder sind zugeordnet. Neuer Test `document.native_schema`:
alle vier Featurearten, drei unabhängige Körper, Reorder/Rename, Kopie/Duplikat,
Revision und ID-Vergabe nach Undo. **65/65 Windows** bestanden (97,71 s),
**292 lokale Quellhashes unverändert**. Produktquellen wurden nicht geändert.

**Offen:** Nutzerantwort auf die D11l-Übertragungsfrage abwarten. Nur
`tests/CMakeLists.txt` und `tests/integration/NativeSchemaTests.cpp` sowie Skript
sind zu übertragen. Archiv: `build/d11l/d11l-source-delta.tar`; Skript/Manifest:
`docs/implementation-plan/evidence/D11l-artifacts`. Danach vollständigen FreeBSD-Lauf,
bidirektionalen nativen Dateiaustausch und Quellhashvergleich abschließen.
Der Windows-Fixture liegt unter `build/d11l/windows-schema.cbf`.
Letzter beidseitig geprüfter Stand bleibt D11k; D11l nicht als vollständig abgenommen markieren.
Siehe [D11l](implementation-plan/evidence/D11l.md) für Feldmatrix und offene Grenzen.

## Nächster Block

Neue verbindliche Nutzeranforderung: einfacher Mehrkörperworkflow nach dem gezeigten
Fusion-Browser, ohne Komponenten-/Baugruppenpflicht. Im Architekturvertrag und K10
aufgenommen, M01 konkretisiert, neue Abnahme **A-BODY-03**. Noch nicht umgesetzt oder
abgenommen. Beim kommenden Schema-/Identitätsreview sicherstellen, dass unabhängige
Körper direkt unter dem Dokument möglich bleiben; den UI-/NewBody-Ausbau über
S03/M01–M04 nicht mit den bereits bestandenen Codec-Tests gleichsetzen.

Nach Abschluss des offenen D11l-Plattformnachweises: **produktiven Dokumentidentitäts-
Lebenszyklus an der CadSession-/Snapshot-Grenze** implementieren. New erzeugt einen
Namensraum, Revision/Wertkopie/Save-As behalten ihn, Duplikat wechselt ihn.
Legacy-Open-Regel ausdrücklich festlegen; Persistenzgrenze zu D13 sichtbar halten.
D11 bleibt offen, bis seine produktiven Identitätsabnahmen erfüllt sind.

Der Autoritäts-/Store-Wechsel folgt gemäß [ADR 0008](adr/0008-ocaf-binary-persistence.md)
und D12/D13. Noch kein produktiver nativer Dispatch: **VOLUND 16 bleibt Produktwriter**.
Die Datei enthält noch keine produktive persistente DocumentId. Kein paralleler
mutabler OCAF-Schattenzustand. Die bestehenden nativen und Produktregressionen erhalten.

## Prüfumgebung

- Windows: `tools/setup-windows.ps1 -SkipConfigure -PassThru` im selben Prozess wie
  Build/Test aufrufen; Buildverzeichnis `build/p02-qt-final`.
- FreeBSD: `eduard@192.168.1.238`, isolierter Baum
  `/home/eduard/src/voelund-p04-20260913`.
- OCCT-Prefix: `<Prüfbaum>/build/d11d-occt/install`; `OpenCASCADE_DIR` ausdrücklich
  auf `<Prefix>/lib/cmake/opencascade` setzen. Das `.tools/occt-freebsd`-Symlinkziel
  bleibt die ursprüngliche gemeinsame Installation, nicht der Prüf-Prefix.
- Acht D11k-Quelldateien und Prüfskript wurden nach ausdrücklicher Nutzerfreigabe
  übertragen. Keine Quelldateien gelöscht. Alle 16 OCCT-Laufzeitauflösungen des
  Extrusionstests zeigen auf den isolierten Prefix.
- Befehle, Quelllisten, Hashes, Dateirundläufe und Logs stehen in `D11k-artifacts`.

## Versionierung

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`cddf45d` enthält D11j, `407def2` die anschließende Pausendokumentation.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
