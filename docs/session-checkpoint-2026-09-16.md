# Fortsetzung – 16. September 2026

## Verbindlicher Stand

D10 abgeschlossen. **D11a–D11h geprüft, D11 insgesamt weiterhin `in-progress`.**
D12/D13 und Meridian-Abnahme L07 bleiben offen. Prüfbasis dieses Blocks: `0d92f48`.
D11h wird gemeinsam mit Belegen, Changelog und dieser Fortsetzungsdoku auf `main`
committed. Den Abschlusscommit über `git log -1 --oneline` feststellen.
Kein Push, kein Release-Tag, keine Terra-Übergabe.

## D11h

- `NativePrimitiveCodec` ersetzt die beiden `NativeBoxCodec`-Quelldateien.
- Boxen und Zylinder mit Join/Cut/Intersect/Separate haben native Parameter,
  explizite Körper-/Feature-IDs und echte OCAF-Eingabereferenzen.
- `Main():10` enthält `Primitives.v1` mit eindeutigen Körpern und deren Featuregruppen;
  `Main():20` enthält die unabhängige Reihenfolge aller Features als Labelreferenzen.
- Reader validiert bekannte Ziele im selben Datenbaum, Reihenfolge, Parameter,
  doppelte IDs, Zyklen und die zur Referenzstruktur passende Körperzuordnung.
- Strukturell gültige, derzeit geometrisch nicht auswertbare Inputs bleiben erhalten.
- Alte `Boxes.v1`-Dateien sind historische Experimentbelege. Keine veröffentlichte
  native Produktdatei ist betroffen. Der aktuelle Adapter erkennt nur das neue Layout.

**61/61 Windows**, **57/57 FreeBSD/Sway**; native Box- und Zylinderdateien in beiden
Richtungen samt Rückschreiben/-lesen bestanden. **282 Quellhashes identisch**.
Alle Builds und Tests beendet. [Evidenz und Artefakte](implementation-plan/evidence/D11h.md).

## Nächster Block

Native **Skizzengeometrie**: Ebene, Element-IDs und Geometrieparameter verlustfrei
abbilden. Danach Constraints und Extrusionen mit Profilreferenzen. Featurefamilien
weiter einzeln gegen immutable Snapshots prüfen; keine nicht unterstützten Daten
stillschweigend verwerfen. Die vorhandenen Schema-Kopf-/Box-/Zylindertests erhalten.

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
  ist weiterhin die ursprüngliche gemeinsame Installation, nicht der Prüf-Prefix.
- D11h-Prüfskript überträgt sieben Quellen und entfernt ausschließlich die zwei
  ersetzten `src/document/NativeBoxCodec`-Dateien im isolierten Prüfbaum.
  Keine System-OCAF-Bibliotheken zumischen; alle 32 Auflösungen beider nativer Tests
  zeigen auf den isolierten Prefix.
- Befehle, Quelllisten, Hashes und Logs stehen in `D11h-artifacts`.

## Versionierung und vorheriger Abschluss

Basisversion `0.1.0`, Changelog unter `Unreleased` aktuell. Produktversion,
Anwendungsschema und OCAF-Containerformat 12 bleiben getrennt.
`0d92f48` enthält den vorherigen Abschluss D11f/g.
[Vortagsnotiz](session-checkpoint-2026-09-15.md) bleibt historischer Nachweis.
