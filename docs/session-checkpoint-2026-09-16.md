# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11m geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. D11m-Implementierung: `9931c10`.
Der nachgeholte FreeBSD-Nachweis ist bestanden und wird mit diesem
Dokumentationscommit ergänzt; `git log -1 --oneline`.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11l – Abschluss

Alle aktuellen Snapshotfelder sind zugeordnet. `document.native_schema` prüft
alle vier Featurearten, drei unabhängige Körper, Reorder/Rename, Kopie/Duplikat,
Revision und ID-Vergabe nach Undo. **65/65 Windows** (97,71 s),
**61/61 FreeBSD/Sway** (34,28 s), **292 Quellhashes identisch**.
Beidseitiger nativer Dateirundlauf bestanden; alle vier Dateien byteidentisch.
Alle Builds und Tests beendet. Produktquellen wurden in D11l nicht geändert.
Siehe [D11l](implementation-plan/evidence/D11l.md) für Feldmatrix und offene Grenzen.

## D11m – Abschluss

CadSession besitzt jetzt DocumentId, `identifiedSnapshot()` und
`duplicateDocument()`. Bearbeitung/Undo/Redo/Save-As behalten die ID; New, Legacy-
Open und explizites Duplikat erzeugen neue. Legacy-Dateien tragen keine ID, daher
auch bei erneutem Open derselben Datei neue ID. Duplikat ist ungespeichert/dirty,
ohne Pfad und flüchtige Historie; Parameter, Zähler und Auswahl bleiben erhalten.
Desktop-Aktion und native dauerhafte Speicherung fehlen noch.

**66/66 Windows** (95,88 s), **62/62 FreeBSD/Sway** (34,61 s).
**294 Quellhashes identisch und unverändert**. Alle Builds und Tests beendet.
Die sieben Quellen samt Skript wurden ausdrücklich freigegeben und übertragen.
Belege: [D11m](implementation-plan/evidence/D11m.md) und `D11m-artifacts`.

## Nächster Block

Neue verbindliche Nutzeranforderung: einfacher Mehrkörperworkflow nach dem gezeigten
Fusion-Browser, ohne Komponenten-/Baugruppenpflicht. Im Architekturvertrag und K10
aufgenommen, M01 konkretisiert, neue Abnahme **A-BODY-03**. Noch nicht umgesetzt oder
abgenommen. Die Schema-/Identitätsprüfung bestätigt, dass unabhängige
Körper direkt unter dem Dokument möglich sind; den UI-/NewBody-Ausbau über
S03/M01–M04 nicht mit den bereits bestandenen Codec-Tests gleichsetzen.

Als nächster Block: explizites Duplizieren
an den bestehenden Desktop-Dokumentablauf anbinden, einschließlich Umgang mit
ungespeicherten Änderungen. Die Anwendungsmethode existiert; keine zweite
Dokumentautorität einführen. Native Persistenz bleibt D12/D13 zugeordnet.
D11 bleibt bis zur vollständigen Identitätsabnahme offen.

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
- Sieben D11m-Quelldateien samt Prüfskript wurden nach ausdrücklicher Freigabe
  übertragen; keine Quelldateien gelöscht. Die Suite nutzt denselben isolierten Prefix.
- Befehle, Quelllisten, Hashes, Dateirundläufe und Logs stehen in `D11m-artifacts`.

## Versionierung

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`cddf45d` enthält D11j, `407def2` die anschließende Pausendokumentation.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
