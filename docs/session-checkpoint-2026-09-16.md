# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11l geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. D11k: `225e7d8`,
Mehrkörperanforderung: `f962bef`, lokaler D11l-Zwischenstand: `422e746`.
D11l wurde nach ausdrücklicher Übertragungsfreigabe auf FreeBSD fertig geprüft.
Dieser Dokumentationscommit ergänzt die fehlenden Belege; `git log -1 --oneline`.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11l – Abschluss

Alle aktuellen Snapshotfelder sind zugeordnet. `document.native_schema` prüft
alle vier Featurearten, drei unabhängige Körper, Reorder/Rename, Kopie/Duplikat,
Revision und ID-Vergabe nach Undo. **65/65 Windows** (97,71 s),
**61/61 FreeBSD/Sway** (34,28 s), **292 Quellhashes identisch**.
Beidseitiger nativer Dateirundlauf bestanden; alle vier Dateien byteidentisch.
Alle Builds und Tests beendet. Produktquellen wurden in D11l nicht geändert.
Siehe [D11l](implementation-plan/evidence/D11l.md) für Feldmatrix und offene Grenzen.

## Nächster Block

Neue verbindliche Nutzeranforderung: einfacher Mehrkörperworkflow nach dem gezeigten
Fusion-Browser, ohne Komponenten-/Baugruppenpflicht. Im Architekturvertrag und K10
aufgenommen, M01 konkretisiert, neue Abnahme **A-BODY-03**. Noch nicht umgesetzt oder
abgenommen. Die Schema-/Identitätsprüfung bestätigt, dass unabhängige
Körper direkt unter dem Dokument möglich sind; den UI-/NewBody-Ausbau über
S03/M01–M04 nicht mit den bereits bestandenen Codec-Tests gleichsetzen.

Als nächster Schritt: **produktiven Dokumentidentitäts-
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
- Zwei D11l-Testdateien und Prüfskript wurden nach ausdrücklicher Nutzerfreigabe
  übertragen. Keine Quelldateien gelöscht. Alle 16 OCCT-Laufzeitauflösungen des
  Schematests zeigen auf den isolierten Prefix.
- Befehle, Quelllisten, Hashes, Dateirundläufe und Logs stehen in `D11l-artifacts`.

## Versionierung

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`cddf45d` enthält D11j, `407def2` die anschließende Pausendokumentation.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
