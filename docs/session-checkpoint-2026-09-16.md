# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11i geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. Prüfbasis dieses Blocks: `572ac5e`.
D11i wird gemeinsam mit Belegen, Changelog und dieser Fortsetzungsdoku auf `main`
committed. Den Abschlusscommit über `git log -1 --oneline` feststellen.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11i

- `NativePrimitiveCodec` unterstützt nun Box, Zylinder und Skizzengeometrie;
  `NativeSketchGeometry` kapselt die private Skizzenkodierung.
- Ebene, Linie/Rechteck/Kreis/Bogen/Ellipse, Element-IDs, Konstruktionsflags,
  Linienrichtungen und beide Vergabezähler bleiben exakt erhalten.
- Die native Stützreferenz zeigt auf ein Solid-Feature. Die Skizze liegt in dessen
  Körpergruppe, erzeugt aber fachlich weiterhin keine eigene Solid-BodyId.
- Gültige historische skalierte Ebenenachsen werden nicht still normalisiert.
- Nichtleere Constraints und Extrusionen werden vor Schreibmutationen abgewiesen.
- `Primitives.v1`, der native Kopf und die Box-/Zylinderkodierung bleiben erhalten.

**62/62 Windows**, **58/58 FreeBSD/Sway**; native Skizzendateien in beiden
Richtungen samt Rückschreiben/-lesen bestanden. **285 Quellhashes identisch**.
Alle Builds und Tests beendet. [Evidenz und Artefakte](implementation-plan/evidence/D11i.md).

## Nächster Block

Native **Skizzen-Constraints**: vollständige Parameter, Referenzen und IDs ohne
stillschweigenden Datenverlust abbilden. Danach Extrusionen mit Profilreferenzen.
Featurefamilien einzeln gegen immutable Snapshots prüfen. Die vorhandenen nativen
Kopf-/Box-/Zylinder-/Skizzenregressionen erhalten.

Erst nach vollständiger Abbildung folgt der Autoritäts-/Store-Wechsel gemäß
[ADR 0008](adr/0008-ocaf-binary-persistence.md) und D12/D13. Noch kein produktiver
nativer Dispatch: **VOLUND 16 bleibt Produktwriter**. Die Datei enthält noch keine
produktive persistente DocumentId. Kein paralleler mutabler OCAF-Schattenzustand.

## Prüfumgebung

- Windows: `tools/setup-windows.ps1 -SkipConfigure -PassThru` im selben Prozess wie
  Build/Test aufrufen; Buildverzeichnis `build/p02-qt-final`.
- FreeBSD: `eduard@192.168.1.238`, isolierter Baum
  `/home/eduard/src/voelund-p04-20260913`.
- OCCT-Prefix: `<Prüfbaum>/build/d11d-occt/install`; `OpenCASCADE_DIR` ausdrücklich
  auf `<Prefix>/lib/cmake/opencascade` setzen. Das `.tools/occt-freebsd`-Symlinkziel
  bleibt die ursprüngliche gemeinsame Installation, nicht der Prüf-Prefix.
- Acht D11i-Quelldateien und Prüfskript wurden nach ausdrücklicher Nutzerfreigabe
  übertragen. Keine Quelldateien gelöscht. Alle 16 OCCT-Laufzeitauflösungen des
  Skizzentests zeigen auf den isolierten Prefix.
- Befehle, Quelllisten, Hashes, Dateirundläufe und Logs stehen in `D11i-artifacts`.

## Versionierung

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`572ac5e` enthält den vorherigen Abschluss D11h.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
